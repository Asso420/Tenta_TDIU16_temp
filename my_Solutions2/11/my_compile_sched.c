/*

a: Problemet uppstår när flera trådar kör "p->max_jobs = max_jobs;" samtidigt, detta orsakar att fler trådar än antal max filer 
    skapas fast trots att de inte behövs. 
b:
  busy waits:
        while (!compile_after->done)      (Väntar på att en viss fil har kompilerats)
            ;

        while (member_of->jobs_running >= member_of->max_jobs)    (Väntar på att tillräckligt få filer kompileras just nu.)
        ;

    while (!all_done);    (Väntar på att alla filer är kompilerade.)
c:
 se koden
d:
se koden
e:
Ja, för att funktionen är rätt synkroniserad med de delade resurser som var kritiska. 
*/

#include "wrap/thread.h"
#include "wrap/synch.h"
#include "wrap/atomics.h"
#include "wrap/timer.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#define MAX_FILES 100

/**
 * En enskild fil att kompilera. Du kan anta att medlemmarna bara
 * modifieras av funktionerna i den här filen.
 */
struct file {
    // Filen som ska kompileras.
    char *filename;

    // Behöver filen kompileras efter någon annan fil?
    struct file *compile_after;

    // Vilket projekt är filen en del av? Sätts av 'project_add'.
    struct project *member_of;

    // Är filen färdigkompilerad?
    bool done;

    struct semaphore file_sema;  // --d
    struct lock file_lock;      // --d
};

// Hjälpfunktion för att skapa en fil.
struct file *file_create(const char *filename) {
    struct file *f = malloc(sizeof(struct file));
    f->filename = strdup(filename); 

    sema_init(&f->file_sema, 0); // --d
    lock_init(&f->file_lock);       // --d

    f->compile_after = NULL;
    f->member_of = NULL;
    f->done = false;
    return f;
}

// Destruering av en fil. Du kan anta att filen inte används någon
// annan stans när den destrueras.
void file_destroy(struct file *f) {
    free(f->filename);
    free(f);
}


/**
 * Ett projekt. Innehåller ett antal filer som alla ska kompileras.
 * Filerna kan bero på varandra (dvs. de måste kompileras i en viss
 * ordning). Du kan anta att medlemmarna bara modifieras av
 * funktionerna i den här filen.
 */
struct project {
    // Alla filer i denna arbetsyta.
    struct file *files[MAX_FILES];

    // Antal filer.
    int file_count;

    // Antal anrop till 'run_compiler' som körs just nu.
    int jobs_running;

    // Maximalt antal anrop till 'run_compiler' som ska få köras
    // samtidigt.
    int max_jobs;

    struct lock max_jobs_lock;

    struct lock Jobs_done_lock;

    struct semaphore sema;
};

// Skapa ett tomt projekt.
struct project *project_create(void) {
    struct project *p = malloc(sizeof(struct project));
    p->file_count = 0;
    lock_init(&p->max_jobs_lock);   // --d
    lock_init(&p->Jobs_done_lock); // --d

    sema_init(&p->sema, 0);        // --c
    return p;
}

// Förstör ett projekt. Du kan anta att ingen annan tråd använder det
// samtidigt.
void project_destroy(struct project *p) {
    for (int i = 0; i < p->file_count; i++) {
        file_destroy(p->files[i]);
    }
    free(p);
}

// Lägg till en fil till ett projekt. Du kan anta följande:
// - Endast en tråd (den här tråden) använder projektet 'to'.
// - Filen som läggs till har inte lagts till i ett annat projekt.
//   Filen är alltså unik för detta projekt.
void project_add(struct project *to, struct file *file) {
    assert(to->file_count < MAX_FILES);
    assert(file->member_of == NULL);

    to->files[to->file_count++] = file;
    file->member_of = to;
}

// Kompilera en enskild fil. Du kan anta att denna bara anropas från
// 'file_compile' nedanför. Denna funktion är dyr att köra, eftersom
// den anropar kompilatorn. Den är implementerad i botten av filen.
static void run_compiler(const char *file);

// Kompilera en enskild fil. Du kan anta att denna funktion bara
// anropas från 'project_compile' nedanför.
static void file_compile(struct file *f) {
    // Vid behov: Vänta på att projektet 'build_after' blir färdigt.


    // denna sema_dow sätts till sema_up när funktione "file_compile" kallas från "project_compile"
    sema_down(&f-file_sema);  // --d   
    struct file *compile_after = f->compile_after;
    if (compile_after) {
        // while (!compile_after->done)   // --d
        //     ;
        compile_after->done;  // --d
    }
    // sema_up sätts tillbaka till sema_down igen efter att tråden är klar
    sema_down(&f-file_sema); // --d
    // Vänta tills det är tillräckligt få andra trådar som kör 'run_compiler':


    // det är samma logik förklarat i raderna ovaan 
    sema_down(&f-file_sema); // --d
    struct project *member_of = f->member_of;
    // while (member_of->jobs_running >= member_of->max_jobs)       // --d
    //     ;
    if (member_of->jobs_running < member_of->max_jobs){             // --d
    // Kompilera filen, meddela andra trådar att vi anropar 'run_compiler'.
    member_of->jobs_running++;
    run_compiler(f->filename);
    member_of->jobs_running--;
    }
    sema_down(&f-file_sema);  // --d

    lock_aquire(&f->file_lock); // --d
    f->done = true;
    lock_release(&f->file_lock);    // --d
}

// Kompilera ett projekt. Kör upp till 'max_jobs' anrop till
// 'run_compiler' samtidigt. Du kan anta följande:
// - Endast en tråd (den här tråden) använder projektet 'p'.
// För full poäng ska det gå att köra 'project_compile' mer
// än en gång.
void project_compile(struct project *p, int max_jobs) {
    // Börja med att återställa 'done' i alla projekt.
    for (int i = 0; i < p->file_count; i++) {
        p->files[i]->done = false;
    }

    // Återställ data i oss själva.
    p->jobs_running = 0;

    // locket gör att flera trådar ska inte kunna läsa in "max_jobs" samtidigt
    lock_aquire(&p->max_jobs_lock);                     // --c
    p->max_jobs = max_jobs;
    lock_release(&p->max_jobs_lock);          // --c

    // Starta en tråd för varje fil.
    for (int i = 0; i < p->file_count; i++) {
        // tanken är att för tråd vi ska skapa sätter vi sema_up, vilket signalerar till de 
        //kretiska delarna inuti "file_compile" som skyddas av sema_down, att släppa in trådar att jobba
        sema_up(&p->sema[i]);                      // --d
        thread_new(&file_compile, p->files[i]);

    }
    sema_up(&p->sema); // vi är klar med semaphoren   // --d


    // Vänta tills alla filer är klara.
    bool all_done;
    do {

        // locket släpper en tråd åt gången
        lock_aquire(&p->Jobs_done_lock);        // --c
        all_done = true;
        for (int i = 0; i < p->file_count; i++) {
            if (!p->files[i]->done) {
                all_done = false;
            }
        }
       lock_release(&p->Jobs_done_lock); // --c
    } 
    // while (!all_done)        // --c
    // ;
}


/**
 * Huvudprogram.
 *
 * Programmet finns för att visa hur koden ovan kan köras. Den är inte
 * nödvändigtvis designad för att visa alla möjliga problem i
 * implementationen.
 *
 * Ändringar nedanför denna kommentar kommer att ignoreras vid
 * bedömning av din lösning. Uppgiften ska alltså lösas utan att lägga
 * till synkronisering här nedanför. Det går självklart bra att
 * modifiera koden nedanför för att testa din lösning.
 */

// Implementation som simulerar att det tar tid att kompilera källkod.
static void run_compiler(const char *file) {
    printf("START: compiling %s...\n", file);
    timer_msleep(1000 + rand() % 300);
    printf("DONE: compiling %s\n", file);
}


int main(void) {
    struct project *p = project_create();

    // Lägg till 10 filer som kan kompileras parallellt:
    for (int i = 0; i < 10; i++) {
        char buffer[10];
        snprintf(buffer, 10, "first/%d", i);
        project_add(p, file_create(buffer));
    }

    // Lägg till en kedja av 5 filer som beror på varandra:
    struct file *dep1 = file_create("dep/1");
    project_add(p, dep1);
    struct file *dep2 = file_create("dep/2");
    dep2->compile_after = dep1;
    project_add(p, dep2);
    struct file *dep3 = file_create("dep/3");
    dep3->compile_after = dep2;
    project_add(p, dep3);
    struct file *dep4 = file_create("dep/4");
    dep4->compile_after = dep3;
    project_add(p, dep4);
    struct file *dep5 = file_create("dep/5");
    dep5->compile_after = dep4;
    project_add(p, dep5);

    // Lägg till 20 till filer som alla beror på 'dep5'.
    for (int i = 0; i < 20; i++) {
        char buffer[10];
        snprintf(buffer, 10, "second/%d", i);
        struct file *f = file_create(buffer);
        f->compile_after = dep5;
        project_add(p, f);
    }

    project_compile(p, 3);

    project_destroy(p);

    return 0;
}
