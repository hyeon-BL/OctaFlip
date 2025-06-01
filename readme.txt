Compile:
    - server: gcc -o server server.c cJSON.c
    - client: gcc -o client client.c cJSON.c


Get started:
    - server: ./server
    - client: ./client -ip 0.0.0.0 -port 5050 -username {username}