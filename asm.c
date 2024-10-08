#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void * teacher_routine(void *);
void * student_routine(void *);
void * tutor_routine(void *);
bool all_students_in_lab(int groupid);
struct tutor_object * dequeue_tutor();
void enqueue_tutor(struct tutor_object* tutor);
bool all_tutors_ready();
int total_students_in_group(int grp_id);
int students_left_per_group(int grp_id);
bool all_tutors_have_left();

int no_of_students; // each has unique ID
int no_of_groups;
int no_of_tutors;
int no_of_labs;
int students_arrived = 0;
int students_left = 0;
int student_id_assigned = 0;
int students_repeated_ids = 0;
int students_entered_lab = 0;
int * group_ids;
int group_id_done = -1;

int curr_tid_to_repeat = -1; // current tutor to repeat leaving 
int curr_id_to_repeat = -1; // current student to repeat their grp id
int curr_grp_id = -1; // current group for teacher 
int curr_lab_num = -1; 
int curr_tutor_id = -2;

int time_limit_each_group;
int lab_availability_status = -1;
int teacher_to_tutor = -2; // -2 to tell tutor that the all students are in the lab
int grps_done = 0;
int tutors_can_start = false;

int waiting_count; // num of waiting groups
int tutors_to_leave = false;
int front = 0, rear = 0, tutor_queue_size = 0; 
int teacher_to_announce = false;
bool all_stud_left = false;


pthread_cond_t waiting_students_arrival; // teacher waits for all students to arrive
pthread_cond_t waiting_teacher_to_give_IDs; // students wait for teacher to assign group
pthread_cond_t waiting_repeating_ids; // teacher waits for students to repeat IDs
pthread_cond_t waiting_students_leave;  // for students to hear from teacher that they can leave now
pthread_cond_t tutor_enter;
pthread_cond_t students_all_left; // teacher need confirmation that all students have left 
pthread_cond_t tutor_available;
pthread_cond_t students_enter_lab;
pthread_cond_t student_check_lab;
pthread_cond_t tutors_vacate_room;
pthread_cond_t announce_grp_arrived;
pthread_cond_t wait_all_tutors_ready;
pthread_cond_t tutors_repeat_leave;
pthread_cond_t tell_tutor_exit;

pthread_mutex_t tutor_enter_mutex= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t grp_id_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t waiting_for_tutors_mutex=PTHREAD_MUTEX_INITIALIZER; // wait for tutors 
pthread_mutex_t students_announce_lab_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t students_mutex;
pthread_mutex_t lab_availability_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t students_left_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t announce_students_left_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t distribute_group_IDs_mutex;
pthread_mutex_t tutor_id_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t tutor_repeat_id_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t tutors_leave_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t tutor_queue_mutex ;  // mutex for queue operations


pthread_t *tutor_thrd_ids; // Tutor system thread ID
pthread_t *s_thrd_ids; // system thread id
pthread_t t_thrd_id; //teacher system thread id



typedef struct student_object {
    int id;
	int group_id;
    bool in_lab;
    bool has_left;
    bool done_lab;
} student_object;

typedef struct tutor_object {
    int tutor_id;
    int group_id;
	int is_ready;
    int lab_id;
    bool has_left;
    bool can_leave;
} tutor_object;


struct student_object *students; //user-defined thread id
struct tutor_object *tutors; //user-defined thread id
struct tutor_object **tutors_queue; 


int main(int argc, char ** argv) {

 	int k, rc;

    // ask for total num of students in class
    printf("Enter the total number of students in class (int): ");
	scanf("%d", &no_of_students);
        printf("\n");

	
	//ask for the number of groups 
	printf("Enter the number of groups (int): ");
	scanf("%d", &no_of_groups);
        printf("\n\n");

    //ask for the number of tutors/labs 
	printf("Enter the number of tutors/lab rooms (int): ");
	scanf("%d", &no_of_tutors);
        printf("\n\n");
    
    //ask for time limit
	printf("Enter the time limit (int): ");
	scanf("%d", &time_limit_each_group);
        printf("\n\n");

    no_of_labs = no_of_tutors;
    waiting_count = no_of_groups;


    s_thrd_ids = malloc((no_of_students) * sizeof(pthread_t)); //student thread ids
	if(s_thrd_ids == NULL){
		fprintf(stderr, "threads out of memory\n");
		exit(1);
	}	

    tutor_thrd_ids = malloc((no_of_tutors) * sizeof(pthread_t)); //tutors thread ids
	if(tutor_thrd_ids == NULL){
		fprintf(stderr, "threads out of memory\n");
		exit(1);
	}	
	
	students = malloc((no_of_students) * sizeof(struct student_object)); //total is no_of_student
	if(students == NULL){
		fprintf(stderr, "t out of memory\n");
		exit(1);
	}	

    group_ids = (int *)malloc(no_of_students * sizeof(int));
    if (group_ids == NULL) {
        fprintf(stderr, "Failed to allocate memory for group IDs\n");
        exit(EXIT_FAILURE);
    }

    tutors = malloc(no_of_tutors * sizeof(struct tutor_object));
    if(tutors == NULL) {
        fprintf(stderr, "tutors out of memory\n");
        exit(1);
    }

    tutors_queue = malloc((no_of_tutors+1) * sizeof(tutor_object*));
    if(tutors_queue == NULL) {
        fprintf(stderr, "tutors_queue out of memory\n");
        exit(1);
    }


    pthread_cond_init(&tutor_available, NULL);
    pthread_cond_init(&students_enter_lab, NULL);
    pthread_cond_init(&student_check_lab, NULL);
    pthread_cond_init(&tutors_vacate_room, NULL);
    pthread_cond_init(&tell_tutor_exit, NULL);
    pthread_cond_init(&announce_grp_arrived, NULL);
    pthread_cond_init(&students_all_left, NULL);
    pthread_cond_init(&waiting_students_leave, NULL);
    pthread_cond_init(&tutors_repeat_leave, NULL);
    pthread_cond_init(&tutor_enter, NULL);

    //Initialize condition variable and mutex objects 
	pthread_mutex_init(&students_mutex, NULL);
	pthread_mutex_init(&tutor_queue_mutex, NULL);
	pthread_mutex_init(&distribute_group_IDs_mutex, NULL);


    rc = pthread_cond_init(&waiting_students_arrival, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_cond_init() (waiting_students_arrival) is %d\n", rc);
		exit(-1);
	}
	rc = pthread_cond_init(&waiting_teacher_to_give_IDs, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_cond_init() (waiting_teacher_to_give_IDs) is %d\n", rc);
		exit(-1);
	}
    rc = pthread_cond_init(&waiting_repeating_ids, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_cond_init() (waiting_repeating_ids) is %d\n", rc);
		exit(-1);
	}

    rc = pthread_cond_init(&waiting_students_leave, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_cond_init() (waiting_students_leave) is %d\n", rc);
		exit(-1);
	}

    
    //create the teacher thread.
	if (pthread_create(&t_thrd_id, NULL, teacher_routine, NULL)){
		printf("ERROR; return code from pthread_create() (teacher) is %d\n", rc);
		exit(-1);
	}

    //create consumers according to the arrival rate
	int n_c;
    int s_c;
    n_c = s_c = 0;
    int id;
   

    for (int k = 0; k < no_of_students; k++) {
        students[k].id = k;
        students[k].group_id = -1;
        students[k].in_lab = false;
        students[k].has_left = false;
        students[k].done_lab = false;
		rc = pthread_create(&s_thrd_ids[k], NULL, student_routine, (void *)&students[k]);
		if (rc) {
			printf("ERROR; return code from pthread_create() (student) is %d\n", rc);
			exit(-1);
		}
    }


   for (int t = 0; t < no_of_tutors; t++) {
        tutors[t].is_ready = false;
        tutors[t].tutor_id = t;
        tutors[t].lab_id = t;
        tutors[t].group_id = -1;
        tutors[t].has_left = false;
        tutors[t].can_leave = false;
        rc = pthread_create(&tutor_thrd_ids[t], NULL, tutor_routine, (void *)&tutors[t]);
        if (rc) {
            fprintf(stderr, "Error: Unable to create tutor thread. Return code: %d\n", rc);
            exit(EXIT_FAILURE);
        }
    }

    // join tutors threads
    for (int k = 0; k<no_of_tutors; k++) {
		pthread_join(tutor_thrd_ids[k], NULL);
    }

    //join student threads
    for (int k = 0; k<no_of_students; k++) {
		pthread_join(s_thrd_ids[k], NULL);
    }
    

    pthread_join(t_thrd_id, NULL); // Wait for the teacher thread to exit

    printf("Main thread: This the end of simulation.\n");

    //After all student threads exited, terminate the teacher thread
    pthread_cancel(t_thrd_id); 
			
	//deallocate allocated memory
	free(tutor_thrd_ids);
	free(s_thrd_ids);
	free(students);
	free(tutors);
    free(group_ids);
    free(tutors_queue);


	//destroy mutex and condition variable objects
    pthread_mutex_destroy(&students_mutex);
	pthread_mutex_destroy(&waiting_for_tutors_mutex);
	pthread_mutex_destroy(&students_announce_lab_mutex);
	pthread_mutex_destroy(&lab_availability_mutex);
	pthread_mutex_destroy(&students_left_mutex);
	pthread_mutex_destroy(&distribute_group_IDs_mutex);
	pthread_mutex_destroy(&tutor_id_mutex);
	pthread_mutex_destroy(&tutor_queue_mutex);
    pthread_mutex_destroy(&announce_students_left_mutex);
    
    
    pthread_cond_destroy(&tutor_enter);
	pthread_cond_destroy(&waiting_students_arrival);
	pthread_cond_destroy(&waiting_teacher_to_give_IDs);
	pthread_cond_destroy(&waiting_repeating_ids);
	pthread_cond_destroy(&waiting_students_leave);
	pthread_cond_destroy(&tutor_available);
	pthread_cond_destroy(&students_enter_lab);
	pthread_cond_destroy(&student_check_lab);
	pthread_cond_destroy(&tutors_vacate_room);
	pthread_cond_destroy(&tell_tutor_exit);
	pthread_cond_destroy(&announce_grp_arrived);
	pthread_cond_destroy(&students_all_left);
	pthread_cond_destroy(&waiting_students_leave);
    pthread_cond_destroy(&tutors_repeat_leave);

    exit(0);
}


void * teacher_routine(void * arg) {
    struct tutor_object *tutor_avail;

    ////////1 
    pthread_mutex_lock(&students_mutex);
    printf("Teacher: I'm waiting for all students to arrive.\n");
    while (students_arrived < no_of_students) {
        // while not all students have arrived yet, continue waiting 
        pthread_cond_wait(&waiting_students_arrival, &students_mutex);
    }
    pthread_mutex_unlock(&students_mutex);

    //////////3
    // once all students arrived
    printf("Teacher: All students have arrived. I start to assign group ids to students.\n");


    // teacher start to assign students 
    pthread_mutex_lock(&distribute_group_IDs_mutex);
    // teacher broadcast to all students: that teacher is ready to assign grp IDs

    int students_per_group = no_of_students / no_of_groups;
    int remainder_students = no_of_students % no_of_groups;

    // to track the number of students assigned to each group
    int group_counts[no_of_groups];
    for (int i = 0; i < no_of_groups; i++) {
        group_counts[i] = students_per_group + (i < remainder_students ? 1 : 0);
    }
    srand(time(0));  

    for (int i = 0; i < no_of_students; ++i) {
        // randomly select a group that still needs students
        int group_id;
        do {
            group_id = rand() % no_of_groups;
        } while (group_counts[group_id] == 0);
        ////////4
        printf("Teacher: student %d is in group %d.\n", i, group_id);
        students[i].group_id = group_id; 
        group_ids[i] = group_id;
        curr_id_to_repeat = i;
        // assign the student to the selected group
        group_counts[group_id]--;
        student_id_assigned++;
        
        pthread_cond_broadcast(&waiting_repeating_ids);
        pthread_cond_wait(&waiting_repeating_ids, &distribute_group_IDs_mutex); 
    }
    pthread_mutex_unlock(&distribute_group_IDs_mutex);

    int gid = 0;

    pthread_cond_broadcast(&tutor_enter);

    while (gid < no_of_groups) {    

        printf("Teacher: I’m waiting for lab room for grp %d to become available.\n", gid); 

        pthread_mutex_lock(&waiting_for_tutors_mutex);
        //wait for lab room to become available  
        tutor_avail = dequeue_tutor();
        if (tutor_avail == NULL) {
            pthread_cond_wait(&tutor_available, &waiting_for_tutors_mutex);
            tutor_avail = dequeue_tutor();
        }

        curr_lab_num = tutor_avail->lab_id;
        curr_tutor_id = tutor_avail->tutor_id;
        pthread_cond_broadcast(&tutors_vacate_room); 

        
        printf("Teacher: The lab [%d] is now available. Students in group %d can enter the room and start your lab exercise.\n", curr_lab_num, gid);
        teacher_to_announce = false;
        pthread_mutex_unlock(&waiting_for_tutors_mutex);



        pthread_mutex_lock(&lab_availability_mutex);
        curr_grp_id = gid;
        tutor_avail->group_id = gid;
        pthread_cond_broadcast(&student_check_lab); // tell student to check if their grp is called 
        lab_availability_status = gid;
        tutor_avail->is_ready = false;

        while (!all_students_in_lab(curr_grp_id)) {
            // teacher checks n waits for all students from grp to enter lab
            pthread_cond_wait(&students_enter_lab, &lab_availability_mutex);
        }
        teacher_to_tutor = curr_grp_id;  // signal to tell tutor that all students have entered

        sleep(1);
        pthread_cond_broadcast(&announce_grp_arrived);
        pthread_mutex_unlock(&lab_availability_mutex);


        //sent all students in lab signal to tutor
        pthread_mutex_lock(&lab_availability_mutex);
        lab_availability_status = -1;
        pthread_mutex_unlock(&lab_availability_mutex);

        pthread_mutex_lock(&tutor_enter_mutex);
        tutors_can_start = false;
        pthread_cond_broadcast(&tutor_enter);
        pthread_mutex_unlock(&tutor_enter_mutex);

        gid++;
    }

    //signal tutor to exit
    pthread_mutex_lock(&students_left_mutex);
    while (students_left != no_of_students) {
        pthread_cond_wait(&students_all_left, &students_left_mutex);
    }
    pthread_mutex_unlock(&students_left_mutex);
    
    printf("Teacher: All students have left.\n");
    pthread_mutex_lock(&tutor_id_mutex);
    curr_tutor_id = -1; // signal to tell tutor that tutor can exit
    curr_lab_num = -1;
    pthread_mutex_unlock(&tutor_id_mutex);
    pthread_cond_broadcast(&tutor_enter);  // to release the while loop 
    pthread_cond_broadcast(&tutors_vacate_room);

    for (int i =0; i < no_of_tutors; i++) {

        pthread_cond_broadcast(&tell_tutor_exit);  // signal tutor to leave

        printf("Teacher: There are no students waiting. Tutor [%d], you can go home now.\n", i);
        while (!tutors[i].has_left && curr_tid_to_repeat != i) {
            // printf("teacher: tutor has not left yet\n");
            // wait for tutor to finish repeating that they are leaving 
            pthread_cond_wait(&tutors_repeat_leave, &tutors_leave_mutex);
        }
        
    }

    // wait until all tutors have left then print the messsage that both stud and tuts left
    pthread_mutex_lock(&tutor_id_mutex);
    while (!all_tutors_have_left()) {
        pthread_cond_wait(&tutors_repeat_leave, &tutors_leave_mutex);
    }   
    pthread_mutex_unlock(&tutor_id_mutex);

    printf("Teacher: All students and tutors are left. I can now go home.\n");
    pthread_exit(EXIT_SUCCESS);
}


bool all_students_in_lab(int groupid)  {
    pthread_mutex_lock(&students_mutex);
    for (int i=0; i < no_of_students; i++) {
        if (students[i].group_id == groupid && !students[i].in_lab) {
            pthread_mutex_unlock(&students_mutex);
            return false;
        }
    }
    pthread_mutex_unlock(&students_mutex);
    return true;
}


bool all_students_have_left(int groupid)  {
    for (int i=0; i < no_of_students; i++) {
        if (students[i].group_id == groupid && !students[i].has_left) {
            return false;
        }
    }
    return true;
}


bool all_tutors_have_left()  {
    for (int i=0; i < no_of_tutors; i++) {
        if (!tutors[i].has_left) {
            return false;
        }
    }
    return true;
}


bool all_students_done_lab(int groupid)  {
    pthread_mutex_lock(&students_left_mutex);
    for (int i=0; i < no_of_students; i++) {
        if (students[i].group_id == groupid && !students[i].done_lab) {
            pthread_mutex_unlock(&students_left_mutex);
            return false;
        }
    }
    pthread_mutex_unlock(&students_left_mutex);
    return true;
}



void * student_routine(void * arg) {
    struct student_object * student = (struct student_object*) arg;
    
    pthread_mutex_lock(&students_mutex);
    printf("Student [%d]: I have arrived and wait for being assigned to a group.\n", student->id);
    students_arrived++;
    if (students_arrived == no_of_students) {
        pthread_cond_signal(&waiting_students_arrival);
    }
    pthread_mutex_unlock(&students_mutex);


    // after all students arrive 
    // student wait for teacher to finish assigning group ids
    pthread_mutex_lock(&distribute_group_IDs_mutex); 
    // for student to repeat the id right after the teacher assigns a student
    while (curr_id_to_repeat != student->id && lab_availability_status == -1) {
        pthread_cond_wait(&waiting_repeating_ids, &distribute_group_IDs_mutex);
    }
    printf("Student [%d]: OK, I'm in group [%d] and waiting for my turn to enter a lab room.\n", student->id, student->group_id);
    pthread_cond_signal(&waiting_repeating_ids); //create new signal for tutor
    pthread_mutex_unlock(&distribute_group_IDs_mutex);

    //wait for teacher to call to enter lab and conduct exercise
    pthread_mutex_lock(&lab_availability_mutex);
    while (curr_grp_id != student->group_id) {
        pthread_cond_wait(&student_check_lab, &lab_availability_mutex);
    }
    printf("Student [%d] in group [%d]: My group is called. I will enter the lab room now.\n\n", student->id, student->group_id); 
    
    student->in_lab = true;
    ///tutor waits for the broadcast signal that all students has entered lab
    pthread_cond_broadcast(&students_enter_lab);
    pthread_mutex_unlock(&lab_availability_mutex);


    pthread_mutex_lock(&students_left_mutex);

    //wait for tutor to call the end of lab exercise
    while (!student->done_lab) {
        pthread_cond_wait(&waiting_students_leave, &students_left_mutex);
    }

    printf("Student %d in group %d: Thanks Tutor. Bye!\n", student->id, student->group_id); 
    students_left++;
    student->has_left = true;

    // Check if this is the last student leaving the group
    if (students_left_per_group(student->group_id) == total_students_in_group(student->group_id)) {
        pthread_cond_broadcast(&students_all_left);
        waiting_count--;
    }
    pthread_mutex_unlock(&students_left_mutex);
    //wait for tutor to call the end of lab exercise 
    pthread_exit(EXIT_SUCCESS);
}


void * tutor_routine(void * arg) {
    struct tutor_object* tutor = (struct tutor_object*) arg;
    // wait until teacher signals tutor to start 
    pthread_mutex_lock(&tutor_enter_mutex);
    pthread_cond_wait(&tutor_enter, &tutor_enter_mutex);
    pthread_mutex_unlock(&tutor_enter_mutex);
        
    while (1) {

        pthread_mutex_lock(&lab_availability_mutex);
        tutor->is_ready = true; // when teacher dequeues, it will set tutor->is ready to true 
        pthread_mutex_unlock(&lab_availability_mutex);

        enqueue_tutor(tutor);  //add tutor to queue

        pthread_mutex_lock(&waiting_for_tutors_mutex); 
        do {
            // if teacher still has grp of students waiting 
            pthread_cond_wait(&tutors_vacate_room, &waiting_for_tutors_mutex);
        } while ((curr_lab_num != tutor->lab_id) && (curr_tutor_id != -1));
        pthread_cond_signal(&tutor_available);
       
        pthread_mutex_lock(&tutor_id_mutex);
        if (curr_tutor_id == -1) {
            tutor->has_left = true;
            curr_tid_to_repeat = tutor->tutor_id; 

            printf("Tutor %d: Thanks Teacher. Bye!\n", tutor->tutor_id);
            pthread_cond_broadcast(&tutors_repeat_leave);
            
            pthread_mutex_unlock(&tutor_id_mutex);
            pthread_mutex_unlock(&waiting_for_tutors_mutex); 
            pthread_exit(NULL);
        }

        printf("Tutor [%d]: The lab room [%d] is vacated and ready for one group.\n", tutor->tutor_id, tutor->tutor_id); 
        // teacher_to_announce = true; // announce that the lab is available

        pthread_mutex_unlock(&tutor_id_mutex);
        //wait for teacher to assign a group of students 
        pthread_mutex_unlock(&waiting_for_tutors_mutex);

        pthread_mutex_lock(&lab_availability_mutex);
        //wait for teacher to signal lab is full
        while (teacher_to_tutor != tutor->group_id) { //wait for teacher signal
            pthread_cond_wait(&announce_grp_arrived, &lab_availability_mutex);
        }
        //after all students in the group have entered the room
        printf("Tutor [%d]: All students in group %d have entered the room. You can start your exercise now.\n", tutor->tutor_id, tutor->group_id);     //students in group gid conduct the lab exercise     
        pthread_mutex_unlock(&lab_availability_mutex);
        tutor->is_ready = false;

        // simulate doing the lab exercise
        int min_time = (time_limit_each_group > 1) ? time_limit_each_group / 2 : 1; 
        int max_time = (time_limit_each_group > 1) ? time_limit_each_group : 2;  
        srand(time(0));  
        int duration = min_time + rand() % (max_time - min_time + 1);  
        sleep(duration); 

        pthread_mutex_lock(&announce_students_left_mutex); 
        for (int i =0; i < no_of_students; i++) {
            if (students[i].group_id == tutor->group_id && !students[i].done_lab) {
                students[i].done_lab = true;
            }
        }
        printf("Tutor [%d]: Students in group [%d] have completed the lab exercise in %d units of time. You may leave this room now.\n", tutor->tutor_id, tutor->group_id, duration);
        pthread_cond_broadcast(&waiting_students_leave);

        pthread_mutex_lock(&grp_id_mutex);
        while (!all_students_have_left(group_id_done)) {
            pthread_cond_wait(&students_all_left, &grp_id_mutex);
        }
        pthread_mutex_unlock(&grp_id_mutex);
        pthread_mutex_unlock(&announce_students_left_mutex);

        tutor->is_ready = true;
    }

    pthread_exit(EXIT_SUCCESS);

}


bool all_tutors_ready()  {
    for (int i=0; i < no_of_tutors; i++) {
        if (!tutors[i].is_ready) {
            return false;
        }
    }
    return true;
}



int total_students_in_group(int grp_id)  {
    int count =0;
    for (int i=0; i < no_of_students; i++) {
        if (students[i].group_id == grp_id) {
            count++;
        }
    }
    return count;
}


int students_left_per_group(int grp_id)  {
    int count =0;
    for (int i=0; i < no_of_students; i++) {
        if (students[i].group_id == grp_id && students[i].has_left) {
            count++;
        }
    }
    return count;
}

void enqueue_tutor(struct tutor_object* tutor) {
    pthread_mutex_lock(&tutor_queue_mutex);
    tutors_queue[rear] = tutor;
    rear = (rear+1) % (no_of_tutors+1);
    pthread_mutex_unlock(&tutor_queue_mutex);
}


struct tutor_object * dequeue_tutor() {
    pthread_mutex_lock(&tutor_queue_mutex);
    if (front == rear) {
        pthread_mutex_unlock(&tutor_queue_mutex);
        return NULL;
    }
    struct tutor_object* tutor = tutors_queue[front];
    front = (front+1) % (no_of_tutors+1);
    pthread_mutex_unlock(&tutor_queue_mutex);

    return tutor;
}