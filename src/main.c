#include "json.h"
#include "network.h"
#include "screen.h"
#include "threads.h"
#include "lock.h"

#include <stdio.h>


mutex *m1;
mutex *m2;

void th1()
{
    mutex_lock(m1);
    printf("%i\n",1);
    mutex_unlock(m1);
}

void th2()
{
    mutex_lock(m2);
    printf("%i\n",2);
    mutex_unlock(m2);
}



int main(int argc, char ** argv)
{

    m1 = mutex_create();
    m2 = mutex_create();

    thread * t1 = thread_create(th1);
    thread * t2 = thread_create(th2);

    thread_join(t1);
    thread_join(t2);

    printf("end.\n");

    thread_release(t1);
    thread_release(t2);
    mutex_release(m1);
    mutex_release(m2);

    /*
      sock s = socket_create("192.168.1.42", 8000);

      packet * pkt = read_packet(s);

      printf("--%s\n",  pkt->boundary);
      printf("Content-Type: %s\n",  pkt->type);
      printf("Content-Length: %ld\n",  pkt->size);
      printf("%s\n",  pkt->payload);

      write_packet(s, pkt);

      free_packet(pkt);

    */
    libmonitors_init();
    int count = 0;
    struct libmonitors_monitor **monitors = malloc(10* sizeof(struct libmonitors_monitor *));
    if(!libmonitors_detect(&count, &monitors)) {
        // Error handling
        printf("error\n");
    }
    for(int i = 0; i < count; i++) {
        printf("screen: %s %dx%d\n",monitors[i]->name, monitors[i]->current_mode->width, monitors[i]->current_mode->height);
    }

    fgetc(stdin);

    return 0;
}
