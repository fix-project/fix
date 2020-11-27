/**
 * ----
 *  Magic String
 * ----
 *  Name of ENCODE (32 bytes)
 * ----
 *  Symbolic name of output (256 bytes)
 * ----
 *  Output of the ENCODE
 * ----
 */

class EncodedBlob {
  private:
    // Name of the blob
    char name[32];
    // Content of the blob
    byte *content;

    bool Encode_completed;
  public: 
    // Constructor
    EncodedBlob() {};
    EncodedBlob(char name[32]) : name(name) {}; 

    // Destructor
    ~EncodedBlob();

    // Load the file into memory
    void *generate_blob();
} 


