class Blob {
  private:
    // Name of the blob
    char name[32];
    // Content of the blob
    byte *content;

    // Memory instance of the file
    // TODO: If input are read-only, and only one output exists for a pair of program and input, there should not be concurrency issue for having one memory_instance. May become an issue if executions can take place on multiple cores in parallel???
    void * memory_instance;
    // Whether cashed in memory
    bool memory_cached;

  public: 
    // Constructor
    Blob() {};
    Blob(char name[32]) : name(name) {}; 

    // Destructor
    ~Blob();

    // Flush the file and computation tree 
    void flush();

    // Load the file into memory
    void *load_to_memory();
} 


