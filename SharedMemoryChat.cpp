/*
 * Shared Memory Chat with GTK 3 GUI
 * Uses: Shared Memory + Semaphores (POSIX) + GTK 3
 * For: Linux Operating Systems (Same Machine Communication)
 * Required: libgtk-3-dev
 */

#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include <gtk/gtk.h>

using namespace std;

#define SHM_NAME "/linux_chat_shm"
#define SEM_NAME "/linux_chat_sem"
#define MAX_MESSAGES 50
#define MSG_SIZE 256

// Shared Memory Structure
struct SharedData {
    int messageCount;
    char messages[MAX_MESSAGES][MSG_SIZE];
};

// === GLOBAL VARIABLES ===
SharedData *pSharedData = nullptr;
sem_t *semaphore = nullptr;
int shm_fd = -1;
atomic<bool> isRunning(true); // Atomic for thread safety
int lastSeenCount = 0;
int userID;
thread readerThread;

// GTK Widgets
GtkWidget *window;
GtkWidget *chatView;
GtkTextBuffer *chatBuffer;
GtkWidget *inputEntry;
GtkWidget *statusLabel;
GtkWidget *sendBtn;

// === ADD MESSAGE TO CHAT ===
void addMessage(const string &msg) {
    gdk_threads_add_idle([](gpointer data) -> gboolean {
        string *message = static_cast<string*>(data);
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(chatBuffer, &end);
        gtk_text_buffer_insert(chatBuffer, &end, ((*message) + "\n").c_str(), -1);

        // Auto-scroll to bottom
        GtkTextMark *mark = gtk_text_buffer_get_insert(chatBuffer);
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(chatView), mark, 0.0, FALSE, 0.0, 1.0);

        delete message;
        return FALSE;
    }, new string(msg));
}

// === READER THREAD ===
void ReaderThread() {
    while (isRunning) {
        if (sem_wait(semaphore) == 0) {
            if (!isRunning) { // Check again after wait
                sem_post(semaphore);
                break;
            }

            int currentCount = pSharedData->messageCount;

            // Handle Wrap-around or Reset
            if (currentCount < lastSeenCount) {
                lastSeenCount = 0;
            }

            if (currentCount > lastSeenCount) {
                // Read new messages
                for (int i = lastSeenCount; i < currentCount; ++i) {
                     // Ensure null termination safe copy
                     char safeMsg[MSG_SIZE];
                     strncpy(safeMsg, pSharedData->messages[i], MSG_SIZE - 1);
                     safeMsg[MSG_SIZE - 1] = '\0';
                     addMessage(string(safeMsg));
                }
                lastSeenCount = currentCount;
            }

            sem_post(semaphore);
        }

        // Sleep to prevent high CPU usage (polling)
        // Check interruption more frequently
        for (int i = 0; i < 10; i++) {
            if (!isRunning) return;
            usleep(10000); // 10ms * 10 = 100ms total
        }
    }
}

// === SEND MESSAGE ===
void on_send_clicked(GtkWidget *widget, gpointer data) {
    const char *text = gtk_entry_get_text(GTK_ENTRY(inputEntry));
    string inputBuffer = text;

    if (!inputBuffer.empty()) {
        if (sem_wait(semaphore) == 0) {
            if (pSharedData->messageCount < MAX_MESSAGES) {
                char msgBuffer[MSG_SIZE];
                snprintf(msgBuffer, MSG_SIZE, "User %d: %s", userID, inputBuffer.c_str());

                // Safe copy
                strncpy(pSharedData->messages[pSharedData->messageCount], msgBuffer, MSG_SIZE - 1);
                pSharedData->messages[pSharedData->messageCount][MSG_SIZE - 1] = '\0';

                pSharedData->messageCount++;
            } else {
                 // Reset if full (simple circular logic or clear)
                 // For this simple project, we just clear to keep it running
                 pSharedData->messageCount = 0;
                 // Add a system message
                 const char* resetMsg = "[System] Chat buffer cleared.";
                 strncpy(pSharedData->messages[0], resetMsg, MSG_SIZE - 1);
                 pSharedData->messageCount = 1;
                 lastSeenCount = 0; // Reset reader cursor
            }
            sem_post(semaphore);
            gtk_entry_set_text(GTK_ENTRY(inputEntry), "");
        }
    }
}

// === HANDLE ENTER KEY ===
gboolean on_input_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        on_send_clicked(NULL, NULL);
        return TRUE;
    }
    return FALSE;
}

// === WINDOW CLOSE ===
void on_window_destroy(GtkWidget *widget, gpointer data) {
    isRunning = false; // Signal thread to stop
    gtk_main_quit();
}

// === MAIN ===
int main(int argc, char *argv[]) {
    userID = getpid() % 1000;

    // 1. Open/Create Shared Memory
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        cerr << "Error: Could not open shared memory" << endl;
        return 1;
    }
    ftruncate(shm_fd, sizeof(SharedData));

    // 2. Map Memory
    pSharedData = (SharedData *)mmap(0, sizeof(SharedData),
                                      PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (pSharedData == MAP_FAILED) {
        cerr << "Error: Could not map shared memory" << endl;
        return 1;
    }

    // 3. Open/Create Semaphore
    semaphore = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (semaphore == SEM_FAILED) {
        cerr << "Error: Could not open semaphore" << endl;
        return 1;
    }

    // Initialize GTK
    gtk_init(&argc, &argv);

    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    char title[100];
    snprintf(title, 100, "Shared Memory Chat - User %d", userID);
    gtk_window_set_title(GTK_WINDOW(window), title);
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 400);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);

    // Main vertical box
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Title label
    GtkWidget *titleLabel = gtk_label_new("SHARED MEMORY CHAT (Semaphore Sync)");
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(titleLabel), attrs);
    gtk_box_pack_start(GTK_BOX(vbox), titleLabel, FALSE, FALSE, 5);

    // Info box (User ID + Status)
    GtkWidget *infoBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(vbox), infoBox, FALSE, FALSE, 5);

    char idText[50];
    snprintf(idText, 50, "Your ID: User %d", userID);
    GtkWidget *idLabel = gtk_label_new(idText);
    gtk_box_pack_start(GTK_BOX(infoBox), idLabel, FALSE, FALSE, 0);

    statusLabel = gtk_label_new("Connected to shared memory");
    gtk_box_pack_end(GTK_BOX(infoBox), statusLabel, FALSE, FALSE, 0);

    // Chat area (scrolled text view)
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 5);

    chatView = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chatView), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(chatView), GTK_WRAP_WORD_CHAR);
    chatBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chatView));
    gtk_container_add(GTK_CONTAINER(scrolled), chatView);

    // Input area (Entry + Send button)
    GtkWidget *inputBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), inputBox, FALSE, FALSE, 5);

    inputEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(inputEntry), "Type your message...");
    gtk_box_pack_start(GTK_BOX(inputBox), inputEntry, TRUE, TRUE, 0);
    g_signal_connect(inputEntry, "key-press-event", G_CALLBACK(on_input_key_press), NULL);

    sendBtn = gtk_button_new_with_label("SEND");
    gtk_box_pack_start(GTK_BOX(inputBox), sendBtn, FALSE, FALSE, 0);
    g_signal_connect(sendBtn, "clicked", G_CALLBACK(on_send_clicked), NULL);

    // Show all widgets
    gtk_widget_show_all(window);

    // Initialize lastSeenCount after mapping
    // We don't want to read old messages from previous runs unless we want history
    // For this assignment, seeing history is probably good.
    sem_wait(semaphore);
    // If it's a "fresh" start (count is huge or negative), reset
    if (pSharedData->messageCount < 0 || pSharedData->messageCount > MAX_MESSAGES) {
         pSharedData->messageCount = 0;
    }
    lastSeenCount = pSharedData->messageCount;
    sem_post(semaphore);

    // Start reader thread
    readerThread = thread(ReaderThread);

    // Run GTK main loop
    gtk_main();

    // Cleanup safely
    if (readerThread.joinable()) {
        readerThread.join();
    }

    munmap(pSharedData, sizeof(SharedData));
    close(shm_fd);
    sem_close(semaphore);

    return 0;
}
