#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */


// lock for mutual exclusion at the intersection
static struct lock *intersectionLock;
// cvs on which vehicles with corresponding destination wait
static struct cv *North;
static struct cv *South;
static struct cv *West;
static struct cv *East;

// Define vehicle struct
typedef struct Vehicle
{
  Direction origin;
  Direction destination;
  Direction blocker_destination;
  int round_waited;
} Vehicle;

// 4 x 4 matrix that keeps track on the vehicles in array
//  where north = 0 can go to 1/2/3
//        east = 1 can go to 0/2/3
//        south = 2 can go to 0/1/3
//        west = 3 can go to 0/1/2
volatile int vehiclesInIntersection[4][4];
volatile struct Vehicle *actualvehicles[4][4];
/*
volatile int northarray[4] = {0};
volatile int southarray[4] = {0};
volatile int eastarray[4] = {0};
volatile int westarray[4] = {0};

vehiclesInIntersection[0] = northarray;
vehiclesInIntersection[1] = eastarray;
vehiclesInIntersection[2] = southarray;
vehiclesInIntersection[3] = westarray;
*/
// number of vehicles in the intersection, updated on each entry/exit
volatile int numVehicle = 0;


/*
 * Check vehicles in intersection at the moment
 *   and if a block is needed, return true
 *   return false otherwise
 *
 */
bool
need_block(Vehicle* v) 
{
  int origin = v->origin;
  int destination = v->destination;
  /*
   * Going from north
   */
  if ((origin == north) && (destination == west)) { // N -> W RIGHT
    
      if (vehiclesInIntersection[1][3] != 0 || 
          vehiclesInIntersection[2][3] != 0) {
        v->blocker_destination = west;
        return true;
      }

  } else if ((origin == north) && (destination == east)) { // N -> E LEFT

      if (vehiclesInIntersection[3][0] != 0 ||
          vehiclesInIntersection[2][0] != 0) {
        v->blocker_destination = north;
        return true;
      } else if (
          vehiclesInIntersection[3][1] != 0 ||
          vehiclesInIntersection[2][1] != 0) {
        v->blocker_destination = east;
        return true;
      } else if (
          vehiclesInIntersection[2][3] != 0 ||
          vehiclesInIntersection[1][3] != 0) {
        v->blocker_destination = west;
        return true;
      } else if (
          vehiclesInIntersection[1][2] != 0) {
        v->blocker_destination = south;
        return true;
      }
  } else if ((origin == north) && (destination == south)) { // N -> S STRAIGHT
      if (vehiclesInIntersection[3][0] != 0) {
        v->blocker_destination = north;
        return true;
      } else if (
          vehiclesInIntersection[3][1] != 0 ||
          vehiclesInIntersection[1][3] != 0 ||
          vehiclesInIntersection[2][3] != 0 ) {
        v->blocker_destination = west;
        return true;
      } else if (
          vehiclesInIntersection[3][2] != 0 ||
          vehiclesInIntersection[1][2] != 0) {
        v->blocker_destination = south;
        return true;
      }
  /*
   * Going from south
   */
  } else if ((origin == south) && (destination == east)) { // S -> E RIGHT
    
      if (vehiclesInIntersection[3][1] != 0 || 
          vehiclesInIntersection[0][1] != 0) {
        v->blocker_destination = east;
        return true;
      }

  } else if ((origin == south) && (destination == west)) { // S -> W LEFT

      if (vehiclesInIntersection[3][0] != 0 ) {
        v->blocker_destination = north;
        return true;
      } else if (
          vehiclesInIntersection[3][1] != 0 ||
          vehiclesInIntersection[0][1] != 0 ) {
        v->blocker_destination = east;
        return true;
      } else if (
          vehiclesInIntersection[0][2] != 0 ||
          vehiclesInIntersection[1][2] != 0 ) {
        v->blocker_destination = south;
        return true;
      } else if (
          vehiclesInIntersection[1][3] != 0 ||
          vehiclesInIntersection[0][3] != 0) {
        v->blocker_destination = west;
        return true;
      }
  } else if ((origin == south) && (destination == north)) { // S -> N STRAIGHT
      if (vehiclesInIntersection[1][0] != 0 ||
          vehiclesInIntersection[3][0] != 0 ) {
        v->blocker_destination = north;
        return true;
      } else if (
          vehiclesInIntersection[0][1] != 0 ||
          vehiclesInIntersection[3][1] != 0 ) {
        v->blocker_destination = east;
        return true;
      } else if (
          vehiclesInIntersection[1][2] != 0 ) {
        v->blocker_destination = south;
        return true;
      } else if (
          vehiclesInIntersection[1][3] != 0) {
        v->blocker_destination = west;
        return true;
      }
  /*
   * Going from east
   */
  } else if ((origin == east) && (destination == north)) { // E -> N RIGHT
    
      if (vehiclesInIntersection[2][0] != 0 || 
          vehiclesInIntersection[3][0] != 0) {
        v->blocker_destination = north;
        return true;
      }

  } else if ((origin == east) && (destination == south)) { // E -> S LEFT

      if (vehiclesInIntersection[2][0] != 0 ||
          vehiclesInIntersection[3][0] != 0 ) {
        v->blocker_destination = north;
        return true;
      } else if (
          vehiclesInIntersection[0][1] != 0 ||
          vehiclesInIntersection[3][1] != 0 ) {
        v->blocker_destination = east;
        return true;
      } else if (
          vehiclesInIntersection[2][3] != 0 ) {
        v->blocker_destination = west;
        return true;
      } else if (
          vehiclesInIntersection[0][2] != 0 ||
          vehiclesInIntersection[3][2] != 0) {
        v->blocker_destination = south;
        return true;
      }
  } else if ((origin == east) && (destination == west)) { // E -> W STRAIGHT
      if (vehiclesInIntersection[0][1] != 0 ) {
        v->blocker_destination = east;
        return true;
      } else if (
          vehiclesInIntersection[0][2] != 0 ) {
        v->blocker_destination = south;
        return true;
      } else if (
          vehiclesInIntersection[0][3] != 0 ||
          vehiclesInIntersection[2][3] != 0 ) {
        v->blocker_destination = west;
        return true;
      } else if (
          vehiclesInIntersection[2][0] != 0 ||
          vehiclesInIntersection[3][0] != 0) {
        v->blocker_destination = north;
        return true;
      }
  /*
   * Going from west
   */
  } else if ((origin == west) && (destination == south)) { // W -> S RIGHT
    
      if (vehiclesInIntersection[0][2] != 0 || 
          vehiclesInIntersection[1][2] != 0) {
        v->blocker_destination = south;
        return true;
      }

  } else if ((origin == west) && (destination == north)) { // W -> N LEFT

      if (vehiclesInIntersection[0][1] != 0) {
        v->blocker_destination = east;
        return true;
      } else if (
          vehiclesInIntersection[0][2] != 0 ||
          vehiclesInIntersection[1][2] != 0) {
        v->blocker_destination = south;
        return true;
      } else if (
          vehiclesInIntersection[1][0] != 0 ||
          vehiclesInIntersection[2][0] != 0) {
        v->blocker_destination = north;
        return true;
      } else if (
          vehiclesInIntersection[1][3] != 0 ||
          vehiclesInIntersection[2][3] != 0) {
        v->blocker_destination = west;
        return true;
      }
  } else if ((origin == west) && (destination == east)) { // W -> E STRAIGHT
      if (vehiclesInIntersection[2][0] != 0) {
        v->blocker_destination = north;
        return true;
      } else if (
          vehiclesInIntersection[0][2] != 0 ||
          vehiclesInIntersection[1][2] != 0) {
        v->blocker_destination = south;
        return true;
      } else if (
          vehiclesInIntersection[0][1] != 0 ||
          vehiclesInIntersection[2][1] != 0) {
        v->blocker_destination = east;
        return true;
      } else if (
          vehiclesInIntersection[2][3] != 0) {
        v->blocker_destination = west;
        return true;
      }
  }
    return false;
}



/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{

  intersectionLock = lock_create("intersectionLock");
  if (intersectionLock == NULL) {
    panic("could not create intersection lock");
  }

  North = cv_create("North");
  if (North == NULL) {
    panic("could not create cv for going north");
  }

  South = cv_create("South");
  if (South == NULL) {
    panic("could not create cv for going south");
  }

  West = cv_create("West");
  if (West == NULL) {
    panic("could not create cv for going west");
  }

  East = cv_create("East");
  if (East == NULL) {
    panic("could not create cv for going east");
  }

  if (vehiclesInIntersection == NULL) {
    panic("intersection matrix failed");
  }

  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  
  KASSERT(intersectionLock != NULL);
  KASSERT(North != NULL);
  KASSERT(South != NULL);
  KASSERT(East != NULL);
  KASSERT(West != NULL);

  lock_destroy(intersectionLock);
  cv_destroy(North);
  cv_destroy(South);
  cv_destroy(West);
  cv_destroy(East);

}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  
  KASSERT(intersectionLock != NULL);
  KASSERT(North != NULL);
  KASSERT(South != NULL);
  KASSERT(East != NULL);
  KASSERT(West != NULL);
  KASSERT(vehiclesInIntersection != NULL);

  // Create new vehicle
  Vehicle *v = kmalloc(sizeof(struct Vehicle));
  v->origin = origin;
  v->destination = destination;
  v->blocker_destination = -1;
  v->round_waited = 0;

  // check if thread needs to block
  lock_acquire(intersectionLock);

    // check stuff and if constarints not met go to sleep
    while (need_block(v)) {
      if (v->blocker_destination == north) {
        cv_wait(North,intersectionLock);
      } else if (v->blocker_destination == east) {
        cv_wait(East, intersectionLock);
      } else if (v->blocker_destination == south) {
        cv_wait(South, intersectionLock);
      } else if (v->blocker_destination == west) {
        cv_wait(West,intersectionLock);
      }
    }
  

    // vehicle goes into intersection
    (vehiclesInIntersection[origin][destination])++;
    actualvehicles[origin][destination] = v;
    numVehicle++;

  lock_release(intersectionLock);

}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  
  KASSERT(intersectionLock != NULL);
  KASSERT(North != NULL);
  KASSERT(South != NULL);
  KASSERT(East != NULL);
  KASSERT(West != NULL);
  KASSERT(vehiclesInIntersection != NULL);

  // check if thread needs to block
  lock_acquire(intersectionLock);

    // vehicle goes out of intersection
    (vehiclesInIntersection[origin][destination])--;
    numVehicle--;
    KASSERT(vehiclesInIntersection[origin][destination] >= 0);
//    actualvehicles[origin][destination] == NULL;

    if (numVehicle == 0) {
      cv_broadcast(North, intersectionLock);
      cv_broadcast(South, intersectionLock);
      cv_broadcast(East, intersectionLock);
      cv_broadcast(West, intersectionLock);
    } else if (destination == 0) {
      cv_signal(North, intersectionLock);
    } else if (destination == 1) {
      cv_signal(East, intersectionLock);
    } else if (destination == 2) {
      cv_signal(South, intersectionLock);
    } else if (destination == 3) {
      cv_signal(West, intersectionLock);
    }

  lock_release(intersectionLock);

}

