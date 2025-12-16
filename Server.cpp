/*
 * Socket Chat Server with GTK 3 GUI
 * Uses: Sockets + Multithreading + GTK 3
 * For: Linux Operating Systems
 * Required: libgtk-3-dev
 */

#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <gtk/gtk.h>

using namespace std;

#define PORT 8080

// === GLOBAL VARIABLES ===
int serverSocket = -1;
int clientSocket = -1;
bool isRunning = true;
bool clientConnected = false;
mutex dataMutex;

// GTK Widgets
GtkWidget *window;
GtkWidget *chatView;
GtkTextBuffer *chatBuffer;
GtkWidget *inputEntry;
GtkWidget *statusLabel;
GtkWidget *sendBtn;

// === HELPER: Safe Close Socket ===
void CloseSocket(int &sock) {
    lock_guard<mutex> lock(dataMutex);
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        sock = -1;
    }
}

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

void setStatus(const string &status) {
    gdk_threads_add_idle([](gpointer data) -> gboolean {
        string *statusStr = static_cast<string*>(data);
        gtk_label_set_text(GTK_LABEL(statusLabel), statusStr->c_str());
        delete statusStr;
        return FALSE;
    }, new string(status));
}

void enableInput(bool enable) {
    gdk_threads_add_idle([](gpointer data) -> gboolean {
        bool *en = static_cast<bool*>(data);
        gtk_widget_set_sensitive(inputEntry, *en);
        gtk_widget_set_sensitive(sendBtn, *en);
        if (*en) gtk_widget_grab_focus(inputEntry);
        delete en;
        return FALSE;
    }, new bool(enable));
}

// === NETWORK THREAD ===
void NetworkThread() {
    // 1. Create Socket
    int sSock = socket(AF_INET, SOCK_STREAM, 0);
    if (sSock == -1) {
        setStatus("Error: Could not create socket");
        return;
    }

    {
        lock_guard<mutex> lock(dataMutex);
        serverSocket = sSock;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Bind
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        setStatus("Error: Bind failed (Port busy?)");
        CloseSocket(serverSocket);
        return;
    }

    // 3. Listen
    if (listen(serverSocket, 1) < 0) {
        setStatus("Error: Listen failed");
        CloseSocket(serverSocket);
        return;
    }

    setStatus("Waiting for client on port 8080...");
    addMessage("[System] Server started. Waiting...");

    // 4. Accept
    sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    int cSock = accept(serverSocket, (sockaddr *)&clientAddr, &clientLen);

    if (cSock >= 0) {
        {
            lock_guard<mutex> lock(dataMutex);
            clientSocket = cSock;
            clientConnected = true;
        }
        setStatus("Client connected!");
        addMessage("[System] Client connected!");
        enableInput(true);
    } else {
        // Accept failed (maybe serverSocket closed)
        CloseSocket(serverSocket);
        return;
    }

    // 5. Receive Loop
    char buffer[1024];
    while (isRunning) {
        int currentSock = -1;
        {
            lock_guard<mutex> lock(dataMutex);
            currentSock = clientSocket;
        }

        if (currentSock < 0) break;

        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(currentSock, buffer, 1023, 0);

        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            addMessage("Client: " + string(buffer));
        } else {
            addMessage("[System] Client disconnected.");
            setStatus("Client disconnected");

            {
                lock_guard<mutex> lock(dataMutex);
                clientConnected = false;
            }
            enableInput(false);
            break; // Break loop to close socket
        }
    }

    // Clean up
    CloseSocket(clientSocket);
    CloseSocket(serverSocket);
}

// === SEND MESSAGE ===
void on_send_clicked(GtkWidget *widget, gpointer data) {
    lock_guard<mutex> lock(dataMutex);
    if (!clientConnected || clientSocket < 0) return;

    const char *text = gtk_entry_get_text(GTK_ENTRY(inputEntry));
    string inputBuffer = text;

    if (!inputBuffer.empty()) {
        // Use MSG_NOSIGNAL to prevent crash if client disconnected
        ssize_t sent = send(clientSocket, inputBuffer.c_str(), inputBuffer.length(), MSG_NOSIGNAL);
        if (sent < 0) {
            // Send failed
             addMessage("[System] Failed to send message.");
        } else {
            addMessage("Me: " + inputBuffer);
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
    isRunning = false;
    // Use CloseSocket to safely close sockets and unblock threads
    CloseSocket(clientSocket);
    CloseSocket(serverSocket);
    gtk_main_quit();
}

// === MAIN ===
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Chat Server (Sockets + Multithreading)");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 400);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);

    // Main vertical box
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Title label
    GtkWidget *titleLabel = gtk_label_new("CHAT SERVER (Sockets + Multithreading)");
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(titleLabel), attrs);
    gtk_box_pack_start(GTK_BOX(vbox), titleLabel, FALSE, FALSE, 5);

    // Status label
    statusLabel = gtk_label_new("Starting server...");
    gtk_widget_set_halign(statusLabel, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), statusLabel, FALSE, FALSE, 5);

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
    gtk_widget_set_sensitive(inputEntry, FALSE);
    gtk_box_pack_start(GTK_BOX(inputBox), inputEntry, TRUE, TRUE, 0);
    g_signal_connect(inputEntry, "key-press-event", G_CALLBACK(on_input_key_press), NULL);

    sendBtn = gtk_button_new_with_label("SEND");
    gtk_widget_set_sensitive(sendBtn, FALSE);
    gtk_box_pack_start(GTK_BOX(inputBox), sendBtn, FALSE, FALSE, 0);
    g_signal_connect(sendBtn, "clicked", G_CALLBACK(on_send_clicked), NULL);

    // Show all widgets
    gtk_widget_show_all(window);

    // Start network thread
    thread netThread(NetworkThread);
    netThread.detach();

    // Run GTK main loop
    gtk_main();

    return 0;
}
