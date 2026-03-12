# Documentation
*First, a list of all functions. Then a breakdown of what each one does.*

---

## Functions
```cpp
onMessage([](int clientID, std::string msg){ });
runServer(int port, std::string password);
int runClient(std::string ip, int port, std::string password);
void sendMsg(std::string msg, int id);
std::string recvMsg(int id);
bool sendFile(const char* filepath, int id);
bool recvFile(std::string folderpath, int id);
```

---

## onMessage
**Server side only.**
Registers a callback that fires every time any connected client sends a message. `clientID` is who sent it, `msg` is what they sent.
```cpp
onMessage([](int clientID, std::string msg){
    std::cout << clientID << ": " << msg << "\n";
});
```

---

## runServer
Starts the server on the given port. Only clients with the correct password can connect. Runs in the background — call it once and it handles everything.
```cpp
runServer(9090, "MY_PASSWORD");
```

---

## runClient
Connects to a server. Returns a file descriptor used for communication. **Immediately after connecting, the server sends a clientID — always receive it before doing anything else.**
```cpp
int client = runClient("127.0.0.1", 9090, "MY_PASSWORD");
std::string msg = recvMsg(client);
int clientID = std::stoi(msg); // store this, you'll need it
```

The `clientID` is what you pass to `sendMsg` when sending from client to server.

---

## sendMsg
Sends a string message. The `id` parameter depends on who's sending:

- **Server → Client**: pass the `clientID` from `onMessage`
- **Client → Server**: pass the `clientID` received right after `runClient`
```cpp
sendMsg("hello", clientID);
```

---

## recvMsg
Receives a message and returns it as a string. Blocks until a message arrives.

- **Client receiving from server**: pass the file descriptor returned by `runClient`
- **Server receiving from clients**: use `onMessage` instead, that's what it's for
```cpp
std::string msg = recvMsg(client);
```

---

## sendFile
Sends a file over an encrypted connection. Returns `true` on success, `false` on failure. Part of NFTP (Novus File Transfer Protocol).
```cpp
sendFile("image.png", clientID);
```

---

## recvFile
Receives a file and saves it to the specified folder. Returns `true` on success, `false` on failure.
```cpp
recvFile("downloads/", client);
```

---

## Full Client Example
For a complete real-world example, see [NovusChat](https://github.com/Nullora/NovusChat) — a terminal chat app built entirely with NovusNet.