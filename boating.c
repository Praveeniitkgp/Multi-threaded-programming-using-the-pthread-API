#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// semaphore structure
typedef struct {
    int value;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} semaphore;

// Global variables
int m, n;              // Number of  boats and visitors
int *BA;               // Boat   availability array
int *BC;               // Boat-visitor   connection array
int *BT;               // Boat ride time array
pthread_mutex_t bmtx;  // Mutex for accessing shared arrays
pthread_barrier_t *BB; // Barriers 
pthread_barrier_t EOS; // End of  session barrier

// Semaphores
semaphore boat;
semaphore rider;

// Counter for visitors who have completed their ride
int visitors_done = 0;

// P (wait) operation for semaphore
void P(semaphore *s) {
    pthread_mutex_lock(&s->mtx);
    s->value--;
    if (s->value < 0) {
        pthread_cond_wait(&s->cv, &s->mtx);
    }
    pthread_mutex_unlock(&s->mtx);
}

// V (signal) operation for semaphore
void V(semaphore *s) {
    pthread_mutex_lock(&s->mtx);
    s->value++;
    if (s->value <= 0) {
        pthread_cond_signal(&s->cv);
    }
    pthread_mutex_unlock(&s->mtx);
}

// Function for boat threads
void *boat_thread(void *arg) {
    int boat_id = *((int *)arg);
    free(arg);
    
    // Initialize the barrier for this boat
    pthread_barrier_init(&BB[boat_id], NULL, 2);
    
    printf("Boat %d Ready\n", boat_id);
    
    while (1) {
        // Signal that a boat is available
        V(&rider);
        
        // Wait for a visitor
        P(&boat);
        
        // Mark the boat as available
        pthread_mutex_lock(&bmtx);
        BA[boat_id] = 1;
        BC[boat_id] = -1;
        pthread_mutex_unlock(&bmtx);
        
        // Wait for a visitor to claim this boat
        pthread_barrier_wait(&BB[boat_id]);
        
        // Get the ride time from the visitor
        pthread_mutex_lock(&bmtx);
        BA[boat_id] = 0; // Mark as unavailable
        int ride_time = BT[boat_id];
        int visitor_id = BC[boat_id];
        pthread_mutex_unlock(&bmtx);
        
        // Ride with the visitor
        printf("Boat %d Start of ride for visitor %d\n", boat_id, visitor_id);
        usleep(ride_time * 100000); // Convert to microseconds (1 minute = 100ms)
        printf("Boat %d End of ride for visitor %d (ride time = %d)\n", boat_id, visitor_id, ride_time);
        
        // Check if all visitors have completed
        pthread_mutex_lock(&bmtx);
        visitors_done++;
        int all_done = (visitors_done == n);
        pthread_mutex_unlock(&bmtx);
        
        if (all_done) {
            pthread_barrier_wait(&EOS);
            return NULL;
        }
    }
    
    return NULL;
}

// Function for visitor threads
void *visitor_thread(void *arg) {
    int visitor_id = *((int *)arg);
    free(arg);
    
    // Decide random visit time and ride time
    int vtime = (rand() % 91) + 30;  // 30 to 120 minutes
    int rtime = (rand() % 46) + 15;  // 15 to 60 minutes
    
    printf("Visitor %d Starts sightseeing for %d minutes\n", visitor_id, vtime);
    
    // Visit other attractions
    usleep(vtime * 100000); // Convert to microseconds (1 minute = 100ms)
    
    printf("Visitor %d Ready to ride a boat (ride time = %d)\n", visitor_id, rtime);
    
    // Signal that a visitor is ready
    V(&boat);
    
    // Wait for a boat
    P(&rider);
    
    // Find an available boat
    int boat_id = -1;
    while (boat_id == -1) {
        pthread_mutex_lock(&bmtx);
        for (int i = 1; i <= m; i++) {
            if (BA[i] && BC[i] == -1) {
                boat_id = i;
                BC[i] = visitor_id;
                BT[i] = rtime;
                break;
            }
        }
        pthread_mutex_unlock(&bmtx);
        
        if (boat_id == -1) {
            // No available boat found, wait briefly and retry
            usleep(1000); // 1ms delay
        }
    }
    
    printf("Visitor %d Finds boat %d\n", visitor_id, boat_id);
    
    // Synchronize with the boat
    pthread_barrier_wait(&BB[boat_id]);
    
    // Ride is completed in the boat thread
    
    // Visitor leaves
    printf("Visitor %d Leaving\n", visitor_id);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <m> <n>\n", argv[0]);
        printf("Where m = number of boats (5-10)\n");
        printf("      n = number of visitors (20-100)\n");
        return 1;
    }
    
    // Parse command-line arguments
    m = atoi(argv[1]);
    n = atoi(argv[2]);
    
    if (m < 5 || m > 10 || n < 20 || n > 100) {
        printf("Invalid input: m must be 5-10 and n must be 20-100\n");
        return 1;
    }
    
    // Initialize random seed
    srand(time(NULL));
    
    // Initialize arrays (using 1-indexed for easier reading)
    BA = (int *)calloc(m + 1, sizeof(int));
    BC = (int *)calloc(m + 1, sizeof(int));
    BT = (int *)calloc(m + 1, sizeof(int));
    BB = (pthread_barrier_t *)malloc((m + 1) * sizeof(pthread_barrier_t));
    
    // Initialize mutex
    pthread_mutex_init(&bmtx, NULL);
    
    // Initialize semaphores
    boat.value = 0;
    pthread_mutex_init(&boat.mtx, NULL);
    pthread_cond_init(&boat.cv, NULL);
    
    rider.value = 0;
    pthread_mutex_init(&rider.mtx, NULL);
    pthread_cond_init(&rider.cv, NULL);
    
    // Initialize end of session barrier
    pthread_barrier_init(&EOS, NULL, 2);  // Main thread and last boat thread
    
    // Create boat threads
    pthread_t *boat_threads = (pthread_t *)malloc((m + 1) * sizeof(pthread_t));
    for (int i = 1; i <= m; i++) {
        int *id = (int *)malloc(sizeof(int));
        *id = i;
        pthread_create(&boat_threads[i], NULL, boat_thread, (void *)id);
    }
    
    // Create visitor threads
    pthread_t *visitor_threads = (pthread_t *)malloc((n + 1) * sizeof(pthread_t));
    for (int i = 1; i <= n; i++) {
        int *id = (int *)malloc(sizeof(int));
        *id = i;
        pthread_create(&visitor_threads[i], NULL, visitor_thread, (void *)id);
    }
    
    // Wait for all visitors to complete
    pthread_barrier_wait(&EOS);
    
    // Clean up
    for (int i = 1; i <= m; i++) {
        pthread_barrier_destroy(&BB[i]);
        pthread_cancel(boat_threads[i]);  // Cancel any remaining boat threads
        pthread_join(boat_threads[i], NULL);
    }
    
    for (int i = 1; i <= n; i++) {
        pthread_join(visitor_threads[i], NULL);
    }
    
    pthread_barrier_destroy(&EOS);
    pthread_mutex_destroy(&bmtx);
    pthread_mutex_destroy(&boat.mtx);
    pthread_cond_destroy(&boat.cv);
    pthread_mutex_destroy(&rider.mtx);
    pthread_cond_destroy(&rider.cv);
    
    free(BA);
    free(BC);
    free(BT);
    free(BB);
    free(boat_threads);
    free(visitor_threads);
    
    return 0;
}