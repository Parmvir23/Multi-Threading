

int main(int argc, char *argv[]){

	pthread_t tid[MAX_THREADS];  
	pthread_attr_t attr[MAX_THREADS];
	int indices[MAX_THREADS][3];
	int i, indexForZero, arraySize, min;

	// Code for parsing and checking command-line arguments
	if(argc != 4){
		fprintf(stderr, "Invalid number of arguments!\n");
		exit(-1);
	}
	if((arraySize = atoi(argv[1])) <= 0 || arraySize > MAX_SIZE){
		fprintf(stderr, "Invalid Array Size\n");
		exit(-1);				
	}
	gThreadCount = atoi(argv[2]);				
	if(gThreadCount > MAX_THREADS || gThreadCount <=0){
		fprintf(stderr, "Invalid Thread Count\n");
		exit(-1);				
	}
	indexForZero = atoi(argv[3]);
	if(indexForZero < -1 || indexForZero >= arraySize){
		fprintf(stderr, "Invalid index for zero!\n");
		exit(-1);
	}

	GenerateInput(arraySize, indexForZero);

	CalculateIndices(arraySize, gThreadCount, indices); 

	// Code for the sequential part
	SetTime();
	min = SqFindMin(arraySize);
	printf("Sequential search completed in %ld ms. Min = %d\n", GetTime(), min);
	

	// Threaded with parent waiting for all child threads
	InitSharedVars();
	SetTime();

       for (int i = 0; i < gThreadCount; i++) {
            pthread_create(&tid[i], NULL, ThFindMin, &indices[i]);
        }
        // Now the main thread waits for all the children to finish.
	for (int i = 0; i < gThreadCount; i++) {
            pthread_join(tid[i], NULL);
	}
        min = SearchThreadMin();
	printf("Threaded FindMin with parent waiting for all children completed in %ld ms. Min = %d\n", GetTime(), min);

	// Multi-threaded with busy waiting (parent continually checking on child threads without using semaphores)
	InitSharedVars();
	SetTime();

	for(int i = 0; i < gThreadCount; i++) {
            pthread_create(&tid[i], NULL, ThFindMin, &indices[i]);
        }
        
        // Parent checking on children
       volatile int weDone = 0; // We need a non critical variable to track finished children.

       // Loop until all the children are done, or one finds a zero.
       while(weDone < gThreadCount) {
            weDone = 0; // If we don'e reset this, one child finished will keep bumping it.
            for(int i = 0; i < gThreadCount; i ++) {
                if(gThreadDone[i]) {
                    if(gThreadMin[i] == 0) {
		        weDone = gThreadCount; // This breaks the loop constantly rechecking all children.
                        break; // Break inner child checking loop.
                    }
                    weDone++;
                }
            }
        }
        for(int i = 0; i < gThreadCount; i++) {
            pthread_cancel(tid[i]);
        } 
        min = SearchThreadMin();
	printf("Threaded FindMin with parent continually checking on children completed in %ld ms. Min = %d\n", GetTime(), min);
	

	// Multi-threaded with semaphores  
	InitSharedVars();

        // Initialize your semaphores here  
        sem_init(&mutex,0,1);
	sem_init(&completed,0,0);
	SetTime();

        for (int i = 0; i < gThreadCount; i++) {
	    pthread_create(&tid[i], NULL, ThFindMinWithSemaphore, &indices[i]);
	}

	sem_wait(&completed);	//detect children processes completed
        
        for(int i = 0; i < gThreadCount; i++) {
	    pthread_cancel(tid[i]);
	}
	min = SearchThreadMin();
	printf("Threaded FindMin with parent waiting on a semaphore completed in %ld ms. Min = %d\n", GetTime(), min);
}


// Write a regular sequential function to search for the minimum value in the array gData
int SqFindMin(int size) {
   
    int min = gData[0]; // Assume first element is the smallest
   
    /* Loop through the array  */
    for (int i = 0; i < size; i++) {
        /* Return if we found 0 */
        if(gData[i] == 0) {
            return 0;
        }        

        /* If current element is smaller then min */
        else if (min > gData[i]) {
            min = gData[i]; // Replace min with current element
        }
    } 
    return min; // Return the smallest element.
}

void* ThFindMin(void *param) {

    int threadNum = ((int*)param)[0];
    int start = ((int*)param)[1];   
    int end = ((int*)param)[2];   // Get end array index for thread.

    for(int i = start; i < end; i++) {
        if(gData[i] < 1) {
	    gThreadMin[threadNum] = 0; // This thread found the zero.
            break;
        }
        else if(gData[i] < gThreadMin[threadNum]) {
            gThreadMin[threadNum] = gData[i]; // Set thread min to current index in chunk. 
        }
    }
    gThreadDone[threadNum] = true; // Set this thread to done. 
    pthread_exit(NULL); // exit thread.*/
}

void* ThFindMinWithSemaphore(void *param) {
   
    int threadNum = ((int*)param)[0]; // Get thread number
    int start = ((int*)param)[1]; // Get start array index for thread.
    int end = ((int*)param)[2]; // Ged end array index for thread.

    for(int i = start; i < end; i++) {
        if(gData[i] < 1) {
            gThreadMin[threadNum] = 0; // This thread found the zero.
            sem_post(&completed); // Post the completed.
            break;
        }
        else if(gData[i] < gThreadMin[threadNum]) {
            gThreadMin[threadNum] = gData[i]; // new smallest...ok
        }
    }    
 
    gThreadDone[threadNum] = true; // Set this thread to done.

    sem_wait(&mutex); // lock critical section
    
    gDoneThreadCount++; // edit global var.
    
    /* Check global var in critical section while it's locked */
    if (gDoneThreadCount == gThreadCount) {
        sem_post(&completed); // post complete if we're the last thread done.
    }
    
    sem_post(&mutex);  // Unlock the critical section.
    
    pthread_exit(NULL); // exit the thread now.

}

int SearchThreadMin() {
    int i, min = MAX_RANDOM_NUMBER + 1;
    for(i =0; i<gThreadCount; i++) {
        if(gThreadMin[i] == 0)
            return 0;
	if(gThreadDone[i] == true && gThreadMin[i] < min)
	    min = gThreadMin[i];
    }
    return min;
}

void InitSharedVars() {
    int i;
    for(i=0; i < gThreadCount; i++) {
        gThreadDone[i] = false;	
        gThreadMin[i] = MAX_RANDOM_NUMBER + 1;	
    }
    gDoneThreadCount = 0;
}

void GenerateInput(int size, int indexForZero) {
    for(int c = 0; c < size; c++) {
        if(c == indexForZero) {
            gData[c] = 0;
        }
        else {
            gData[c] = GetRand(1, MAX_RANDOM_NUMBER);
        }
    }
}

void CalculateIndices(int arraySize, int thrdCnt, int indices[MAX_THREADS][3]) {
    
    int d = arraySize/thrdCnt; 
    int start = 0;
    int end = d - 1;
   
     for (int i = 0; i < thrdCnt; i++) {
	indices[i][0] = i;
	indices[i][1] = start;
	indices[i][2] = end; 
        start += d; //bump start index
        end += d; // bump end index.
    }

}

// Get a random number in the range [x, y]
int GetRand(int x, int y) {
    int r = rand();
    r = x + r % (y-x+1);
    return r;
}
long GetMilliSecondTime(struct timeb timeBuf) {
    long mliScndTime;
    mliScndTime = timeBuf.time;
    mliScndTime *= 1000;
    mliScndTime += timeBuf.millitm;
    return mliScndTime;
}
long GetCurrentTime(void) {
    long crntTime=0;
    struct timeb timeBuf;
    ftime(&timeBuf);
    crntTime = GetMilliSecondTime(timeBuf);
    return crntTime;
}
void SetTime(void) {
    gRefTime = GetCurrentTime();
}
long GetTime(void) {
    long crntTime = GetCurrentTime();
    return (crntTime - gRefTime);
}
