#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// 4 cond var
// 2 mutex

// dont have one cond var for each group 
// teacher waits for students to arrive 
// students tell techer thier id 

// cond
/// 1. for making the teacher wait for all students to arrive
/// 2. for making students wait until the teacher is ready to give teacher their IDs
/// 3. one for making the teacher wait for students to finish repeating the IDs they just receied
/// 4. oen for making students wait for the teacher to tell them to leave


/// teacher thread calls broadcast 
// 1. each student need to wait for the teacher to tell them their id 
// 2. for teacher: to wait for the students to receive the next instructions once student told her the student id 
/// teacher wait for student to get ready 
/// teacher to dismiss students (students wait for teacher to dismiss students) -> pthread_broadcast (used in 2 places)
// ///// teacher thread calls broadcast 


// mutex
// one to control students leaving and entering 
// another one : to control student and techer for saying / listening student IDs
///////// to manage the distribution of IDs


void * teacher_routine(void *);
void * student_routine(void *);
void * tutor_routine(void *);
struct tutor_object * check_tutor_availability();
bool all_students_in_lab(int groupid);

int no_of_students; // each has unique ID
int no_of_groups;
int no_of_tutors;
int no_of_labs;
int students_arrived = 0;
int students_left = 0;
int student_id_assigned = 0;
int students_repeated_ids = 0;
int students_entered_lab = 0;
int * students_in_group;
int * group_ids;


int curr_id_to_repeat = -1; // current student to repeat their grp id
int curr_grp_id = -1; // current group for teacher 
int num_of_tutors_ready = 0;
int time_limit_each_group;
int curr_tutor_id = 0;
int curr_lab_num = -1; 
int lab_availability_status = -1;
int teacher_announced = false;
int teacher_to_tutor = -1; // to tell tutor that the all students are in the lab

pthread_cond_t waiting_students_arrival; // teacher waits for all students to arrive
pthread_cond_t waiting_teacher_to_give_IDs; // students wait for teacher to assign group
pthread_cond_t waiting_repeating_ids; // teacher waits for students to repeat IDs
pthread_cond_t waiting_students_leave;  // for students to hear from teacher that they can leave now
// students wait for teacher's signal to leave

pthread_cond_t students_all_left; // teacher need confirmation that all students have left 
pthread_cond_t tutor_available;
pthread_cond_t lab_available;
pthread_cond_t students_enter_lab;
pthread_cond_t student_repeat_lab;
pthread_cond_t tutors_vacate_room;
pthread_cond_t finish_assigning;
pthread_cond_t students_in_lab;

pthread_mutex_t students_in; // wait for all students to be in the lab for curr grp
pthread_mutex_t gid_mutex;
pthread_mutex_t grp_id_mutex;
pthread_mutex_t waiting_for_tutors_mutex; // wait for tutors 
pthread_mutex_t students_announce_lab_mutex;
pthread_mutex_t students_mutex;
pthread_mutex_t lab_availability_mutex;
pthread_mutex_t tutor_mutex;
pthread_mutex_t students_left_mutex;
pthread_mutex_t distribute_student_IDs_mutex;
pthread_mutex_t distribute_group_IDs_mutex;


pthread_t *tutor_thrd_ids; // Tutor system thread ID
pthread_t *s_thrd_ids; // system thread id
pthread_t t_thrd_id; //teacher system thread id

typedef struct student_object {
    int id;
	int group_id;
    bool in_lab;
} student_object;

typedef struct tutor_object {
    int tutor_id;
    int group_id;
	int is_ready;
} tutor_object;

// only one teacher
struct student_object *students; //user-defined thread id
struct tutor_object *tutors; //user-defined thread id

int main(int argc, char ** argv) {

    // // only one teacher
    // student_obj *students; //user-defined thread id
    // tutor_obj *tutors; //user-defined thread id

 	int k, rc;


    // ask for total num of students in class
    printf("Enter the total number of students in class (int): ");
	scanf("%d", &no_of_students);
        printf("\n");

	
	//ask for the number of groups 
	printf("Enter the number of groups (int): ");
	scanf("%d", &no_of_groups);
        printf("\n\n");
    
    //ask for time 
	// printf("Enter the time limit (int): ");
	// scanf("%d", &time_limit_each_group);
    //     printf("\n\n");

    time_limit_each_group = 2;
    no_of_tutors = 2;
    no_of_labs = no_of_tutors;

    students_in_group = (int *)malloc(no_of_groups * sizeof(int));
    if (students_in_group == NULL) {
        perror("Failed to allocate memory for students_in_group");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < no_of_groups; i++) {
        students_in_group[i] = 0;
    }

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

    tutors = calloc(no_of_tutors, sizeof(struct tutor_object));
    if(tutors == NULL) {
        fprintf(stderr, "tutors out of memory\n");
        exit(1);
    }

    //Initialize condition variable and mutex objects 
	pthread_mutex_init(&students_mutex, NULL);
	pthread_mutex_init(&distribute_student_IDs_mutex, NULL);
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
		rc = pthread_create(&s_thrd_ids[k], NULL, student_routine, (void *)&students[k]);
		if (rc) {
			printf("ERROR; return code from pthread_create() (student) is %d\n", rc);
			exit(-1);
		}
    }

    int no_of_tutors = 1;

   for (int t = 0; t < no_of_tutors; t++) {
        tutors[t].is_ready = false;
        tutors[t].tutor_id = t;
        tutors[t].group_id = -1;
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
    

    // pthread_join(t_thrd_id, NULL); // Wait for the teacher thread to exit

    printf("Main thread: This the end of simulation.\n");

    //After all student threads exited, terminate the teacher thread
    // pthread_cancel(t_thrd_id); 
			
	//deallocate allocated memory
	free(s_thrd_ids);
	// free(tutor_thrd_ids);
	free(t_thrd_id);
	free(students);
	// free(tutors);
    free(group_ids);
    free(students_in_group);


	//destroy mutex and condition variable objects
    pthread_mutex_destroy(&students_mutex);
	pthread_mutex_destroy(&distribute_student_IDs_mutex);
	pthread_mutex_destroy(&distribute_group_IDs_mutex);


	pthread_cond_destroy(&waiting_students_arrival);
	pthread_cond_destroy(&waiting_teacher_to_give_IDs);
	pthread_cond_destroy(&waiting_repeating_ids);
	pthread_cond_destroy(&waiting_students_leave);


    exit(0);
}


void * teacher_routine(void * arg) {

    ////////1 
    pthread_mutex_lock(&students_mutex);
    printf("Teacher: I'm waiting for all students to arrive.\n");

    while (students_arrived < no_of_students) {
        // while not all students have arrived yet, continue waiting 
        pthread_cond_wait(&waiting_students_arrival, &students_mutex);
    }

    //////////3
    // once all students arrived
    printf("Teacher: All students have arrived. I start to assign group ids to students.\n");
    pthread_mutex_unlock(&students_mutex);


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
        // Assign the student to the selected group
        group_counts[group_id]--;
        students_in_group[group_id]++;
        student_id_assigned++;
        
        pthread_cond_broadcast(&waiting_repeating_ids);
        pthread_cond_wait(&waiting_repeating_ids, &distribute_group_IDs_mutex); //TODO: 
    }
    pthread_mutex_unlock(&distribute_group_IDs_mutex);

    pthread_mutex_lock(&gid_mutex);
    int gid = 0; 
    pthread_mutex_unlock(&gid_mutex);

    while (gid < no_of_groups) {    
        //wait for lab room to become available  
        ///////6
        printf("Teacher: I’m waiting for lab room to become available.\n");   

        pthread_mutex_lock(&gid_mutex);

        // once the teacher knows a tutor is ready then
        pthread_mutex_lock(&waiting_for_tutors_mutex);
        curr_lab_num = gid;
        pthread_cond_broadcast(&tutors_vacate_room); 

        // wait for tutors to be ready (wait for tutor to announce that the lab room is ready)
        struct tutor_object *tutor_avail = check_tutor_availability();
        while (tutor_avail == NULL) {
            pthread_cond_wait(&tutor_available, &waiting_for_tutors_mutex);
            tutor_avail = check_tutor_availability();
        }
        curr_grp_id = gid;
        pthread_mutex_unlock(&waiting_for_tutors_mutex);
        
        printf("Teacher: The lab [%d] is now available. Students in group %d can enter the room and start your lab exercise.\n", curr_lab_num, gid);   
        sleep(1);
        
        // teacher_announced = true;
        pthread_mutex_lock(&lab_availability_mutex);
        //curr_tutor_id = tutor_avail->tutor_id;
        tutor_avail->group_id = curr_grp_id;
        pthread_cond_broadcast(&student_repeat_lab);
        pthread_cond_wait(&students_in_lab, &students_in);

        lab_availability_status = gid;
        // printf("teacher broadcast\n");
        tutor_avail->is_ready = false;

        pthread_mutex_lock(&lab_availability_mutex);
        while (all_students_in_lab(gid)) {
            pthread_cond_wait(&students_enter_lab, &lab_availability_mutex);
        }
        teacher_to_tutor = gid; // 
        pthread_mutex_unlock(&lab_availability_mutex);
        //sent all students in lab signal to tutor
        pthread_mutex_unlock(&gid_mutex);

        // pthread_mutex_unlock(&waiting_for_tutors_mutex);
        

        // signal tutor to take group gid for exercise
        // pthread_mutex_lock(&lab_availability_mutex);
        // tutor_avail->is_ready = false;
        //////////8
        //signal students in group gid to enter and start exercise   
        // printf("Teacher: The lab [%d] is now available. Students in group %d can enter the room and start your lab exercise.\n", curr_lab_num, gid);   
        // signal to all students that those whos grp id is called, to enter the lab 
        // pthread_cond_broadcast(&student_repeat_lab);
        // pthread_mutex_unlock(&lab_availability_mutex);

        pthread_mutex_lock(&lab_availability_mutex);
        lab_availability_status = -1;
        pthread_mutex_unlock(&lab_availability_mutex);

        pthread_mutex_lock(&gid_mutex);
        gid++;
        printf("teacher: moving on to the next gid which is now: %d\n", gid);

        pthread_mutex_unlock(&gid_mutex);

    }


    // while (students_left < no_of_students) {
    //     pthread_mutex_lock(&students_mutex);
    //     pthread_cond_wait(&students_all_left, &students_mutex);
    //     pthread_mutex_unlock(&students_mutex);
    // }

    ////////13
    printf("Teacher: There are no students waiting. Tutor [%d], you can go home now.\n", curr_tutor_id);
    // signal tutor to exit 

    ///////15 
    printf("Teacher: All students and tutors are left. I can now go home.\n");
    pthread_exit(EXIT_SUCCESS);
}


struct tutor_object * check_tutor_availability() {
    // whichever tutor is ready 
    for (int i =0; i < no_of_tutors; i++) {
        if (tutors[i].is_ready) {
            return &tutors[i];
        }
    }
    return NULL;
}

bool all_students_in_lab(int groupid)  {
    pthread_mutex_lock(&students_mutex);
    for (int i; i < no_of_students; i++) {
        if (students[i].group_id == groupid && !students[i].in_lab) {
            return false;
        }
    }
    pthread_mutex_unlock(&students_mutex);
    return true;
}


void * student_routine(void * arg) {
    struct student_object * student = (struct student_object*) arg;
    
    /////////2
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
    // use the mutex for grp id 
    // int group_id = group_ids[student->id]; // either use this or the student->group_id
     // either use this or the student->group_id
    //////////5
    printf("Student [%d]: OK, I'm in group [%d] and waiting for my turn to enter a lab room.\n", student->id, student->group_id);
    pthread_cond_signal(&waiting_repeating_ids); //create new signal for tutor
    pthread_mutex_unlock(&distribute_group_IDs_mutex);

    //wait for teacher to call to enter lab and conduct exercise
    pthread_mutex_lock(&gid_mutex);
    pthread_mutex_lock(&lab_availability_mutex);
    while (curr_grp_id != student->group_id) {
        pthread_cond_wait(&student_repeat_lab, &lab_availability_mutex);
    }
    // printf("Student [%d]: %d %d\n", student->id, curr_grp_id, student->group_id);
    //////////9
    printf("Student [%d] in group [%d]: My group is called. I will enter the lab room now.\n", student->id, student->group_id); 
    // all students from this group has been called
    pthread_mutex_unlock(&lab_availability_mutex);
    pthread_mutex_unlock(&gid_mutex);

    // //signal tutor after all students in group are in lab  
    // pthread_mutex_lock(&lab_availability_mutex);
    // // printf("students in group %d has %d\n\n", gid, students_in_group[group_id]);
    // if (students_entered_lab == students_in_group[student->group_id]) {
    //     // if the students from the same lab has all entered the lab
    //     pthread_cond_signal(&students_enter_lab); // signalled all students from same grp entered the lab 
    // }
    // pthread_mutex_unlock(&lab_availability_mutex);

    

    // for students to leave now
    // multiple students in multple labs //....
    pthread_mutex_lock(&students_mutex);

    pthread_cond_wait(&waiting_students_leave, &students_mutex);
    students_left++;
    if (students_left == no_of_students) {
        pthread_cond_signal(&students_all_left);
    }
    ////////12
    //wait for tutor to call the end of lab exercise 
    printf("Student %d in group %d: Thanks Tutor. Bye!\n", student->id, student->group_id); 
    pthread_mutex_unlock(&students_mutex);

    pthread_exit(EXIT_SUCCESS);
}


void * tutor_routine(void * arg) {
    struct tutor_object* tutor = (struct tutor_object*) arg;
    
    int grp_id = -1;

    while (1) {

        pthread_mutex_lock(&waiting_for_tutors_mutex);
        // num_of_tutors_ready++;
        while (curr_lab_num == -1) {
            pthread_cond_wait(&tutors_vacate_room, &waiting_for_tutors_mutex);
        }
        //wait for teacher to assign a group of students 
        int exercise_time = time_limit_each_group;
        printf("Tutor [%d]: The lab room [%d] is vacated and ready for one group.\n", tutor->tutor_id, curr_lab_num); 
        ////////7
        tutor->is_ready = true;
        num_of_tutors_ready++;
        pthread_cond_signal(&tutor_available); // signal to teacher that the lab room is ready 
        pthread_mutex_unlock(&waiting_for_tutors_mutex);


        pthread_mutex_lock(&lab_availability_mutex);
        //wait for all students in group gid to enter lab 
        ////////10
        printf("tutor: current group is %d\n", curr_grp_id);
        // pthread_mutex_lock(&lab_availability_mutex);
        //wait for teacher to signal lab is full

        while (teacher_to_tutor == tutor->group_id) { //wait for teacher signal
            pthread_cond_wait(&students_enter_lab, &lab_availability_mutex);
        }
        pthread_mutex_unlock(&lab_availability_mutex);


        //after all students in the group have entered the room
        printf("Tutor [%d]: All students in group %d have entered the room. You can start your exercise now.\n", tutor->tutor_id, curr_grp_id);     //students in group gid conduct the lab exercise     
        pthread_mutex_unlock(&lab_availability_mutex);

        pthread_mutex_lock(&waiting_for_tutors_mutex);
        tutor->is_ready = false;
        pthread_mutex_unlock(&waiting_for_tutors_mutex);


        sleep(exercise_time); // simulate doing the lab exercise


        pthread_mutex_lock(&gid_mutex);
        pthread_mutex_lock(&waiting_for_tutors_mutex);   
        ////////11
        printf("Tutor [%d]: Students in group %d have completed the lab exercise in %d units of time. You may leave this room now.\n", tutor->tutor_id, curr_grp_id, time_limit_each_group);    
        pthread_mutex_unlock(&waiting_for_tutors_mutex);
        pthread_mutex_unlock(&gid_mutex);


        // signal the students to leave 
        pthread_mutex_lock(&students_mutex);
        // should only broadcast to the grp thats done
        pthread_cond_broadcast(&waiting_students_leave);
        pthread_mutex_unlock(&students_mutex);

        while (students_left < no_of_students) {
            pthread_mutex_lock(&students_mutex);
            pthread_cond_wait(&students_all_left, &students_mutex);
            pthread_mutex_unlock(&students_mutex);
        }

        pthread_mutex_lock(&waiting_for_tutors_mutex);
        tutor->is_ready = false;
        pthread_cond_signal(&tutor_available);
        pthread_mutex_unlock(&waiting_for_tutors_mutex);


        //get group id   
        

        //////14
        //if signalled by teacher to exit    
        // printf("Tutor: Thanks Teacher. Bye!\n");    
        // exit     


    
        //wait for lab to become empty     
        //signal teacher the room is vacated
    }

    pthread_exit(EXIT_SUCCESS);


}