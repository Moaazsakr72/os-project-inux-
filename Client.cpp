/*
 * Socket Chat Client with GTK 3 GUI
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
int clientSocket = -1;
bool isRunning = true;
bool isConnected = false;
mutex dataMutex;

// GTK Widgets
GtkWidget *window;
GtkWidget *chatView;
GtkTextBuffer *chatBuffer;
GtkWidget *inputEntry;
GtkWidget *ipEntry;
GtkWidget *statusLabel;
GtkWidget *connectBtn;
GtkWidget *sendBtn;

// === ADD MESSAGE TO CHAT ===
void addMessage(const string &msg) {
    // Must be called from GTK main thread using g_idle_add
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

// === RECEIVE THREAD ===
void ReceiveThread() {
    char buffer[1024];
    while (isRunning && isConnected) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, 1023, 0);

        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            addMessage("Server: " + string(buffer));
        } else {
            addMessage("[System] Disconnected from server.");
            setStatus("Disconnected");
            isConnected = false;
            break;
        }
    }
    close(clientSocket);
}

// === CONNECT TO SERVER ===
void on_connect_clicked(GtkWidget *widget, gpointer data) {
    if (isConnected) return;
    
    const char *ipText = gtk_entry_get_text(GTK_ENTRY(ipEntry));
    string ipBuffer = ipText;
    
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        setStatus("Error: Could not create socket");
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, ipBuffer.c_str(), &serverAddr.sin_addr);

    setStatus("Connecting to " + ipBuffer + "...");
    addMessage("[System] Connecting...");

    if (connect(clientSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        setStatus("Connection failed!");
        addMessage("[System] Connection failed!");
        close(clientSocket);
        return;
    }

    isConnected = true;
    setStatus("Connected to " + ipBuffer);
    addMessage("[System] Connected!");

    // Enable send button and input
    gtk_widget_set_sensitive(sendBtn, TRUE);
    gtk_widget_set_sensitive(inputEntry, TRUE);
    gtk_widget_grab_focus(inputEntry);

    thread t(ReceiveThread);
    t.detach();
}

// === SEND MESSAGE ===
void on_send_clicked(GtkWidget *widget, gpointer data) {
    if (!isConnected) return;
    
    const char *text = gtk_entry_get_text(GTK_ENTRY(inputEntry));
    string inputBuffer = text;
    
    if (!inputBuffer.empty()) {
        send(clientSocket, inputBuffer.c_str(), inputBuffer.length(), 0);
        addMessage("Me: " + inputBuffer);
        gtk_entry_set_text(GTK_ENTRY(inputEntry), "");
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

gboolean on_ip_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        on_connect_clicked(NULL, NULL);
        return TRUE;
    }
    return FALSE;
}

// === WINDOW CLOSE ===
void on_window_destroy(GtkWidget *widget, gpointer data) {
    isRunning = false;
    if (isConnected) {
        close(clientSocket);
    }
    gtk_main_quit();
}

// === MAIN ===
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Chat Client (Sockets + Multithreading)");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 400);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    
    // Main vertical box
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // Title label
    GtkWidget *titleLabel = gtk_label_new("CHAT CLIENT (Sockets + Multithreading)");
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(titleLabel), attrs);
    gtk_box_pack_start(GTK_BOX(vbox), titleLabel, FALSE, FALSE, 5);
    
    // Connection area (IP + Connect button)
    GtkWidget *connBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), connBox, FALSE, FALSE, 5);
    
    GtkWidget *ipLabel = gtk_label_new("Server IP:");
    gtk_box_pack_start(GTK_BOX(connBox), ipLabel, FALSE, FALSE, 0);
    
    ipEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ipEntry), "127.0.0.1");
    gtk_box_pack_start(GTK_BOX(connBox), ipEntry, TRUE, TRUE, 0);
    g_signal_connect(ipEntry, "key-press-event", G_CALLBACK(on_ip_key_press), NULL);
    
    connectBtn = gtk_button_new_with_label("CONNECT");
    gtk_box_pack_start(GTK_BOX(connBox), connectBtn, FALSE, FALSE, 0);
    g_signal_connect(connectBtn, "clicked", G_CALLBACK(on_connect_clicked), NULL);
    
    // Status label
    statusLabel = gtk_label_new("Enter IP and click Connect");
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
    
    // Run GTK main loop
    gtk_main();
    
    return 0;
}
