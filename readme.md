# Assumptions:

1. Stop sharing can only be done by the owner of the group, that decides if the file should stop sharing.
2. Upload works, like having the entire file stored as chunks in its runtime memory, and not storing the file location, as it could be in temporary storage system and be deleted, which could lead to a big loss.
3. The upload file has been modified to be more interactive and easier to use
4. While tracker works effortlessly with pthread, the client doesn't work with downloads as much as expected due to the use of thread header. I believe it can be improved with using pthread header instead