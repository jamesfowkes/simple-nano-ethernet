#include <stdint.h>
#include <string.h>

#include <Wire.h>

#include "raat.hpp"
#include "raat-buffer.hpp"

#include "raat-oneshot-timer.hpp"
#include "raat-oneshot-task.hpp"
#include "raat-task.hpp"

#include "http-get-server.hpp"

typedef struct _timeout
{
    int32_t time;
    bool active;
} timeout;

static HTTPGetServer s_server(NULL);
static const raat_devices_struct * s_pDevices = NULL;

#if NRELAYS == 2
#define INITIAL_RELAY_STATES false, false, false, false
#define INITIAL_TIMEOUT_STATES {0, false}, {0, false}, \
{0, false}, {0, false}
#elif NRELAYS == 4
#define INITIAL_RELAY_STATES false, false, false, false, false, false
#define INITIAL_TIMEOUT_STATES {0, false}, {0, false}, \
{0, false}, {0, false}, {0, false}, {0, false}
#elif NRELAYS == 8
#define INITIAL_RELAY_STATES false, false, false, false, false, false, false, false, false, false
#define INITIAL_TIMEOUT_STATES {0, false}, {0, false}, \
{0, false}, {0, false}, {0, false}, {0, false}, {0, false}, {0, false}, {0, false}, {0, false}
#else
#error "NRELAYS expected to be 2, 4 or 8"
#endif

#define MAX_OUTPUT (NRELAYS + 1)

static bool s_output_states[NRELAYS+2]
{
    // 0 and 1 are ignored - having them in the array make indexing easier
    INITIAL_RELAY_STATES
};

static timeout s_timeouts[NRELAYS+2]
{
    INITIAL_TIMEOUT_STATES
};

typedef void (*output_setter_fn)(int32_t);

static void do_set(int32_t output_pin)
{
    raat_logln_P(LOG_APP, PSTR("Setting output %" PRIi32), output_pin);
    s_output_states[output_pin] = true;
    s_timeouts[output_pin].active = false;
    digitalWrite(output_pin, HIGH);
}

static void do_toggle(int32_t output_pin)
{
    raat_logln_P(LOG_APP, PSTR("Toggling output %" PRIi32), output_pin);
    s_output_states[output_pin] = !s_output_states[output_pin];
    s_timeouts[output_pin].active = false;
    digitalWrite(output_pin, s_output_states[output_pin] ? HIGH : LOW);
}

static void do_clear(int32_t output_pin)
{
    raat_logln_P(LOG_APP, PSTR("Clearing output %" PRIi32), output_pin);
    s_output_states[output_pin] = false;
    s_timeouts[output_pin].active = false;
    digitalWrite(output_pin, LOW);
}

static void start_timeout(int32_t timeout, int32_t output_pin)
{
    raat_logln_P(LOG_APP, PSTR("Starting %" PRIi32 " timeout on output %" PRIi32), timeout, output_pin);
    s_timeouts[output_pin].time = (timeout / 100) * 100;;
    s_timeouts[output_pin].active = true;
}

static void send_standard_erm_response()
{
    s_server.set_response_code_P(PSTR("200 OK"));
    s_server.set_header_P(PSTR("Access-Control-Allow-Origin"), PSTR("*"));
    s_server.finish_headers();
}

static void get_input(char const * const url, char const * const end)
{
    (void)url;

    int32_t input_pin;
    char const * const pInputPin = end;

    bool success = false;

    send_standard_erm_response();

    if ((success = raat_parse_single_numeric(pInputPin, input_pin, NULL)))
    {
        success = ((input_pin >= MIN_INPUT) && (input_pin <= MAX_INPUT));

        if (success)
        {
            if (digitalRead(A0+input_pin) == HIGH)
            {
                s_server.add_body_P(PSTR("1\r\n\r\n"));
            }
            else
            {
                s_server.add_body_P(PSTR("0\r\n\r\n"));   
            }
        }
    }

    if (!success)
    {
        s_server.add_body_P(PSTR("?\r\n\r\n"));
    }
}

static void set_bool_response(bool success)
{
    if (success)
    {
        s_server.add_body_P(PSTR("OK\r\n\r\n"));
    }
    else
    {
        s_server.add_body_P(PSTR("?\r\n\r\n"));
    }
}

static void nontimed_output_handler(char const * const pOutputUrl, output_setter_fn pOutputSetterFn)
{
    int32_t output_pin;

    bool success = false;

    send_standard_erm_response();

    if ((success = raat_parse_single_numeric(pOutputUrl, output_pin, NULL)))
    {
        success = ((output_pin >= 2) && (output_pin <= (NRELAYS+2)));
        if (success)
        {
            pOutputSetterFn(output_pin);
        }
    }
    set_bool_response(success);
}

static void timed_output_handler(char const * const pOutputUrl, output_setter_fn pOutputSetterFn)
{
    int32_t output_pin;
    int32_t timeout;

    char * pTime = NULL;

    bool success = false;

    send_standard_erm_response();

    if ((success = raat_parse_single_numeric(pOutputUrl, output_pin, &pTime)))
    {
        if ((success = raat_parse_single_numeric(pTime+1, timeout, NULL)))
        {
            success = ((output_pin >= 2) && (output_pin <= (NRELAYS+2)));
            success &= timeout > 100;

            if (success)
            {
                pOutputSetterFn(output_pin);
                start_timeout(timeout, output_pin);
            }
        }
    }
    set_bool_response(success);
}

static void set_output(char const * const url, char const * const end)
{
    (void)url;
    nontimed_output_handler(end+1, do_set);
}

static void toggle_output(char const * const url, char const * const end)
{
    (void)url;
    nontimed_output_handler(end+1, do_toggle);
}

static void clear_output(char const * const url, char const * const end)
{
    (void)url;
    nontimed_output_handler(end+1, do_clear);
}

static void timed_set_output(char const * const url, char const * const end)
{
    (void)url;
    timed_output_handler(end+1, do_set);
}


static void timed_toggle_output(char const * const url, char const * const end)
{
    (void)url;
    timed_output_handler(end+1, do_toggle);
}

static void timed_clear_output(char const * const url, char const * const end)
{
    (void)url;
    timed_output_handler(end+1, do_clear);
}


static const char GET_INPUT_URL[] PROGMEM = "/input/get";
static const char SET_OUTPUT_URL[] PROGMEM = "/output/set";
static const char TOGGLE_OUTPUT_URL[] PROGMEM = "/output/toggle";
static const char CLEAR_OUTPUT_URL[] PROGMEM = "/output/clear";
static const char TIMED_SET_OUTPUT_URL[] PROGMEM = "/output/timedset";
static const char TIMED_TOGGLE_OUTPUT_URL[] PROGMEM = "/output/timedtoggle";
static const char TIMED_CLEAR_OUTPUT_URL[] PROGMEM = "/output/timedclear";

static http_get_handler s_handlers[] = 
{
    {GET_INPUT_URL, get_input},
    {SET_OUTPUT_URL, set_output},
    {TOGGLE_OUTPUT_URL, toggle_output},
    {CLEAR_OUTPUT_URL, clear_output},
    {TIMED_SET_OUTPUT_URL, timed_set_output},
    {TIMED_TOGGLE_OUTPUT_URL, timed_toggle_output},
    {TIMED_CLEAR_OUTPUT_URL, timed_clear_output},
    {"", NULL}
};

void ethernet_packet_handler(char * req)
{
    s_server.handle_req(s_handlers, req);
}

char * ethernet_response_provider()
{
    return s_server.get_response();
}

static void timeout_task_fn(RAATTask& task, void * pTaskData)
{
    (void)task; (void)pTaskData;
    for (uint8_t output = MIN_OUTPUT; output <= MAX_OUTPUT; output++)
    {
        if (s_timeouts[output].active && s_timeouts[output].time > 0)
        {
            s_timeouts[output].time -= 100;   
            if (s_timeouts[output].time == 0)
            {
                raat_logln_P(LOG_APP, PSTR("Timeout finish on output %" PRIu8), output);
                do_toggle(output);
            }
        }
    }
}
static RAATTask s_timeout_task(100, timeout_task_fn);

void raat_custom_setup(const raat_devices_struct& devices, const raat_params_struct& params)
{
    (void)params;

    s_pDevices = &devices;

    for (uint8_t i = MIN_OUTPUT; i <= MAX_OUTPUT; i++)
    {
        pinMode(i, OUTPUT);
    }

    for (uint8_t i = MIN_INPUT; i <= MAX_INPUT; i++)
    {
        pinMode(i, INPUT_PULLUP);
    }

    uint8_t default_on_outputs[] = DEFAULT_ON;
    const uint8_t number_of_on_outputs = sizeof(default_on_outputs);
    for (uint8_t i=0; i<number_of_on_outputs; i++)
    {
        do_set(default_on_outputs[i]);
    }
    raat_logln_P(LOG_APP, PSTR("Simple Nano Ethernet: ready"));
}

void raat_custom_loop(const raat_devices_struct& devices, const raat_params_struct& params)
{
    (void)devices; (void)params;
    s_timeout_task.run();
}
