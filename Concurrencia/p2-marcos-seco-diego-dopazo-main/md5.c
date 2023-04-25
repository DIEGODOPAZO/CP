#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <openssl/evp.h>


#include "options.h"
#include "queue.h"


#define MAX_PATH 1024
#define BLOCK_SIZE (10*1024*1024)
#define MAX_LINE_LENGTH (MAX_PATH * 2)


struct file_md5 {
    char *file;
    unsigned char *hash;
    unsigned int hash_size;
};

struct dirThreadData{
    char * dir;
    queue q;
};

struct computeHashData{
    queue in_q;
    queue out_q;
};

struct writeFileData{
    int dirname_len;
    queue out_q;
    char *filename;
};


struct readHashData{
    char *dir;
    char *file;
    queue q;
};

void get_entries(char *dir, queue q);


void print_hash(struct file_md5 *md5) {
    for(int i = 0; i < md5->hash_size; i++) {
        printf("%02hhx", md5->hash[i]);
    }
}


void read_hash_file(char *file, char *dir, queue q) {
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char *file_name, *hash;
    int hash_len;

    if((fp = fopen(file, "r")) == NULL) {
        printf("Could not open %s : %s\n", file, strerror(errno));
        exit(0);
    }

    while(fgets(line, MAX_LINE_LENGTH, fp) != NULL) {
        struct file_md5 *md5 = malloc(sizeof(struct file_md5));
        file_name = strtok(line, ": ");
        hash      = strtok(NULL, ": ");
        hash_len  = strlen(hash);

        md5->file      = malloc(strlen(file_name) + strlen(dir) + 2);
        sprintf(md5->file, "%s/%s", dir, file_name);
        md5->hash      = malloc(hash_len / 2);
        md5->hash_size = hash_len / 2;


        for(int i = 0; i < hash_len; i+=2)
            sscanf(hash + i, "%02hhx", &md5->hash[i / 2]);

        q_insert(q, md5);
    }
    noMoreElements(q);

    fclose(fp);
}


void sum_file(struct file_md5 *md5) {
    EVP_MD_CTX *mdctx;
    int nbytes;
    FILE *fp;
    char *buf;

    if((fp = fopen(md5->file, "r")) == NULL) {
        printf("Could not open %s\n", md5->file);
        md5->file = NULL;
        return;
    }

    buf = malloc(BLOCK_SIZE);
    const EVP_MD *md = EVP_get_digestbyname("md5");

    mdctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(mdctx, md, NULL);

    while((nbytes = fread(buf, 1, BLOCK_SIZE, fp)) >0)
        EVP_DigestUpdate(mdctx, buf, nbytes);

    md5->hash = malloc(EVP_MAX_MD_SIZE);
    EVP_DigestFinal_ex(mdctx, md5->hash, &md5->hash_size);

    EVP_MD_CTX_destroy(mdctx);
    free(buf);
    fclose(fp);
}


void recurse(char *entry, void *arg) {
    queue q = * (queue *) arg;
    struct stat st;

    stat(entry, &st);

    if(S_ISDIR(st.st_mode))
        get_entries(entry, q);
}


void add_files(char *entry, void *arg) {
    queue q = * (queue *) arg;
    struct stat st;

    stat(entry, &st);

    if(S_ISREG(st.st_mode))
        q_insert(q, strdup(entry));
}


void walk_dir(char *dir, void (*action)(char *entry, void *arg), void *arg) {
    DIR *d;
    struct dirent *ent;
    char full_path[MAX_PATH];

    if((d = opendir(dir)) == NULL) {
        printf("Could not open dir %s\n", dir);
        return;
    }

    while((ent = readdir(d)) != NULL) {
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") ==0)
            continue;

        snprintf(full_path, MAX_PATH, "%s/%s", dir, ent->d_name);

        action(full_path, arg);
    }

    closedir(d);
}


void get_entries(char *dir, queue q) {
    walk_dir(dir, add_files, &q);
    walk_dir(dir, recurse, &q);
}



void *get_entries_wrapper(void *args){

    struct dirThreadData * dirData = (struct dirThreadData *) args;
    char *dir = dirData->dir;
    queue q = dirData->q;

    get_entries(dir, q);

    noMoreElements(q);

    return NULL;
}


void *read_hashes_wraper(void * args){
    struct readHashData * data = (struct readHashData * ) args;

    read_hash_file(data->file, data->dir, data->q);

    return NULL;
}

void *comparar_hashes(void * args){
    queue q = (queue) args;
    struct file_md5 *md5_in, md5_file;


    while((md5_in = q_remove(q))) {
        md5_file.file = md5_in->file;

        sum_file(&md5_file);
        if(md5_file.file == NULL){
            free(md5_in->file);
            free(md5_in->hash);
            free(md5_in);
            continue;
        }

        if(memcmp(md5_file.hash, md5_in->hash, md5_file.hash_size)!=0) {
            printf("File %s doesn't match.\nFound:    ", md5_file.file);
            print_hash(&md5_file);
            printf("\nExpected: ");
            print_hash(md5_in);
            printf("\n");
        }

        free(md5_file.hash);

        free(md5_in->file);
        free(md5_in->hash);
        free(md5_in);
    }

    return NULL;

}

void check(struct options opt) {
    queue in_q;
    pthread_t readHashes;
    pthread_t compareHashes[opt.num_threads];
    struct readHashData data;

    in_q  = q_create(opt.queue_size);

    data.q = in_q;
    data.file = opt.file;
    data.dir = opt.dir;

    if(pthread_create(&readHashes, NULL, read_hashes_wraper, (void *) &data) != 0){
        printf("ERROR while creating the thread\n");
        return;
    }

    for(int i = 0; i < opt.num_threads; i++){

        if(pthread_create(&compareHashes[i], NULL, comparar_hashes, (void *) in_q) != 0){
            printf("ERROR while creating the thread\n");
            return;
        }

    }



    pthread_join(readHashes, NULL);

    for(int i = 0; i < opt.num_threads; i++){

        pthread_join(compareHashes[i], NULL);

    }

    q_destroy(in_q);
}

void * computeHashes(void *args){

    struct computeHashData * hashData = (struct computeHashData *) args;
    char *ent;
    struct file_md5 *md5;

    queue in_q = hashData->in_q;
    queue out_q = hashData->out_q;

    while((ent = q_remove(in_q)) != NULL) {
        md5 = malloc(sizeof(struct file_md5));

        md5->file = ent;
        sum_file(md5);

        q_insert(out_q, md5);

    }

    return NULL;

}



void * writeFile(void *args){
    struct writeFileData * writeData = (struct writeFileData *) args;

    int dirname_len = writeData->dirname_len;
    queue out_q = writeData->out_q;
    char *filename = writeData->filename;
    struct file_md5 *md5;
    FILE *out;

    if((out = fopen(filename, "w")) == NULL) {
        printf("Could not open output file\n");
        exit(0);
    }

    while((md5 = q_remove(out_q)) != NULL) {
        fprintf(out, "%s: ", md5->file + dirname_len);

        for(int i = 0; i < md5->hash_size; i++)
            fprintf(out, "%02hhx", md5->hash[i]);
        fprintf(out, "\n");

        free(md5->file);
        free(md5->hash);
        free(md5);
    }

    fclose(out);

    return NULL;
}


void sum(struct options opt) {
    queue in_q, out_q;

    struct dirThreadData dirData;
    struct computeHashData hashData;
    struct writeFileData writeData;
    pthread_t dirThread;
    pthread_t computeHashThread[opt.num_threads];
    pthread_t writeFileThread;

    in_q  = q_create(opt.queue_size);
    out_q = q_create(opt.queue_size);


    dirData.q = in_q;
    dirData.dir = opt.dir;

    hashData.in_q = in_q;
    hashData.out_q = out_q;

    writeData.dirname_len = strlen(opt.dir) + 1; // length of dir + /
    writeData.filename = opt.file;
    writeData.out_q = out_q;

    if(pthread_create(&dirThread, NULL, get_entries_wrapper, (void *) &dirData) != 0){
        printf("ERROR while creating the thread\n");
        return;
    }

    for(int i = 0; i < opt.num_threads; i++){

        if(pthread_create(&computeHashThread[i], NULL, computeHashes, (void *) &hashData) != 0){
            printf("ERROR while creating the thread\n");
            return;
        }
    }

    if(pthread_create(&writeFileThread, NULL, writeFile, (void *) &writeData) != 0){
        printf("ERROR while creating the thread\n");
        return;
    }



    pthread_join(dirThread, NULL);


    for(int i = 0; i < opt.num_threads; i++){
        pthread_join(computeHashThread[i], NULL);
        if(i == opt.num_threads - 1){
            noMoreElements(out_q);  //ask if here is where it has to be the noMoreElements(out)
        }

    }

    pthread_join(writeFileThread, NULL);






    q_destroy(in_q);
    q_destroy(out_q);
}


int main(int argc, char *argv[]) {

    struct options opt;

    opt.num_threads = 5;
    opt.queue_size  = 1000;
    opt.check       = true;
    opt.file        = NULL;
    opt.dir         = NULL;

    read_options (argc, argv, &opt);

    if(opt.check)
        check(opt);
    else
        sum(opt);

}
