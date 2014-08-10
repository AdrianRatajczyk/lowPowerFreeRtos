#ifndef DRIVER_H_
#define DRIVER_H_



typedef enum {RANDOM, IMMEDIATE, SEARCH} strategy_t;

//Driver configuration

struct driver_configuration {
	const char * driver_name;

	strategy_t driver_strategy;
};

/**
/+InitDriver(bufferSize: int): int/
/+ReleaseDriver(): int/
/+SendCommand(command : char): int/
/+ReciveCommand(): char/
/+FlushBuffer(): int/
/+RedirectOutput(extBuffer: char*): int/
/+StopRedirect(): int/
 */
 struct driver_t {
    /**
     */
    int (* init_driver) (struct driver_t * , int, int);
    /**
     */
    int (* release_driver) (struct driver_t * );
    /**
     */
    int (* send_command) ( struct driver_t * , char);
    /**
     */
    int (* recive_command) ( struct driver_t * );
    /**
     */
    int (* flush_buffer) ( struct driver_t * );
    /**
     */
    int (* redirect_output) ( struct driver_t * , char*);
    /**
     */
    int (* stop_redirect) ( struct driver_t * );

	/* private data */
    int id;
    long time_out;
    char* buffer;
    strategy_t driver_strategy;
};

 extern "C"  struct driver_t * new_driver(struct driver_configuration * );

#endif



