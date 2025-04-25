
#include "timer.h"
#include <stdio.h>
#include <stdlib.h>

static pthread_t _timer;

struct timer_id_container_t { //* A linked list to store all devices
	struct timer_id_t id;
	struct timer_id_container_t * next;
};

static struct timer_id_container_t * dev_list = NULL;

static uint64_t _time;

static int timer_started = 0;
static int timer_stop = 0;


static void * timer_routine(void * args) {
	while (!timer_stop) {
		printf("Time slot %3lu\n", current_time());
		int fsh = 0;   //* To track the number of devices that have finished their job	
		int event = 0; //* To track the number of devices that have done their job
		
		/* Wait for all devices have done the job in current
		 * time slot */
		struct timer_id_container_t * temp;

		/* Let devices continue their job */
		for (temp = dev_list; temp != NULL; temp = temp->next) {
			pthread_mutex_lock(&temp->id.timer_lock);
			temp->id.done = 0;
			pthread_cond_signal(&temp->id.timer_cond);
			pthread_mutex_unlock(&temp->id.timer_lock);
		}
		for (temp = dev_list; temp != NULL; temp = temp->next) {
			pthread_mutex_lock(&temp->id.event_lock);
			while (!temp->id.done && !temp->id.fsh) {
				pthread_cond_wait( //*this function wait for event_cond to be signaled, during this time, the mutex event_lock is unlocked to let other threads access the event_lock
					&temp->id.event_cond,
					&temp->id.event_lock
				);
			}
			if (temp->id.fsh) {
				fsh++;
			}
			event++;
			pthread_mutex_unlock(&temp->id.event_lock);
		}

		/* Increase the time slot */
		_time++;
		

		if (fsh == event) {
			break;
		}
	}
	pthread_exit(args);
}

//* This function is called by each device to tell the timer that it has done, and ready to go to the next time slot
void next_slot(struct timer_id_t * timer_id) {
	/* Tell to timer that we have done our job in current slot */
	pthread_mutex_lock(&timer_id->event_lock);
	timer_id->done = 1;
	pthread_cond_signal(&timer_id->event_cond);
	pthread_mutex_unlock(&timer_id->event_lock);

	/* Wait for going to next slot */
	pthread_mutex_lock(&timer_id->timer_lock);
	while (timer_id->done) { //* Wait while done is 1, once all devices have done their job, the timer will set done to 0
		pthread_cond_wait(
			&timer_id->timer_cond,
			&timer_id->timer_lock
		);
	}
	pthread_mutex_unlock(&timer_id->timer_lock);
}

uint64_t current_time() {
	return _time;
}

void start_timer() {
	timer_started = 1;
	pthread_create(&_timer, NULL, timer_routine, NULL);
}

void detach_event(struct timer_id_t * event) {
	pthread_mutex_lock(&event->event_lock);
	event->fsh = 1;
	pthread_cond_signal(&event->event_cond);
	pthread_mutex_unlock(&event->event_lock);
}

struct timer_id_t * attach_event() {
	if (timer_started) { //*This mean that if the timer has started, we can not attach a new event
		return NULL;
	}else{
		struct timer_id_container_t * container =
			(struct timer_id_container_t*)malloc(
				sizeof(struct timer_id_container_t)		
			); //* This create a node in the linked list (a node contains a timer_id_t and a pointer to the next node)
		container->id.done = 0;
		container->id.fsh = 0;
		pthread_cond_init(&container->id.event_cond, NULL);
		pthread_mutex_init(&container->id.event_lock, NULL);
		pthread_cond_init(&container->id.timer_cond, NULL);
		pthread_mutex_init(&container->id.timer_lock, NULL);
		if (dev_list == NULL) { //* Add the first node to the linked list of devices (events)
			dev_list = container;
			dev_list->next = NULL;
		}else{
			container->next = dev_list;
			dev_list = container;
		}
		return &(container->id);
	}
}

void stop_timer() {
	timer_stop = 1;
	pthread_join(_timer, NULL);
	while (dev_list != NULL) {
		struct timer_id_container_t * temp = dev_list;
		dev_list = dev_list->next;
		pthread_cond_destroy(&temp->id.event_cond);
		pthread_mutex_destroy(&temp->id.event_lock);
		pthread_cond_destroy(&temp->id.timer_cond);
		pthread_mutex_destroy(&temp->id.timer_lock);
		free(temp);
	}
}




