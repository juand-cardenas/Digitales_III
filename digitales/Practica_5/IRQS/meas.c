/** @file meas.c @brief Implementación de la medición (sin divisiones). */
#include "meas.h"
#include "config.h"
#include "timebase.h"
#include "hardware/sync.h"

//toma toda la info que veo
typedef struct {
    bool     started, fell; //me dice el flanco de subida y bajada (si ocurrio o no).
    uint32_t win_t0, rise_t, fall_t; //tiempo de la ultima subida y bajda
    uint32_t periods, th_sum, last_edge_t; //periodo suma del timepo en alto y ultimo flanco
    bool     pub_valid;
    meas_result_t pub;
    uint32_t pub_t;
} meas_t;

static meas_t m;   /**< Escrita por la ISR de flancos; leída bajo sección crítica. */

void meas_init(uint32_t t)
{
    m = (meas_t){0}; //pone toda ltodas las estrucutras en cero
    m.last_edge_t = t; //marca el instante de arranque como el ultimo flanco, con espera de 500ms
}

void meas_on_edge(bool level, uint32_t t)
{
    if ((uint32_t)(t - m.last_edge_t) > TIMEOUT_US) { //el ultimo flanco fue hace 500ms,
                                                        // y no parecio otro
        m.started   = false; //descarta la ventana en curso
        m.pub_valid = false; //descarda los datso
    }
    m.last_edge_t = t; //actulizo sobre el ultimo flanco

    if (level) {                               /* subida flanco de subida */
        if (!m.started) { //pogo en cero los contadores para empezar a contar desde ya
            m.started = true;
            m.win_t0  = t;
            m.periods = 0;
            m.th_sum  = 0;
        } else if (m.fell) {                   /* periodo completo */
            m.th_sum += m.fall_t - m.rise_t; //suma el tiepo en alto  del periodo
            m.periods++; //cuanta el periodo
            uint32_t span = t - m.win_t0; //duracion de la ventata, desde la primera subida hasta esta otra
            if (span >= WINDOW_MIN_US) { // si la venta duro 50 ms se cierra
                m.pub.periods = m.periods;
                m.pub.th      = m.th_sum;
                m.pub.span    = span;
                m.pub_t       = t;
                m.pub_valid   = true;
                m.win_t0      = t;
                m.periods     = 0;
                m.th_sum      = 0;
                //lo anterior son los datos a publicar.
            }
        }
        m.rise_t = t; //tiene el timepo en el que estuvo el ultimo flanco
        m.fell   = false;
    } else {                                   /* flanco de bajada */
        if (m.started && !m.fell) { //mientar no se halla gurado un flanco 
                                     //de bajda en esta ventan se toma
            m.fall_t = t;
            m.fell   = true;
        }
    }
}

meas_state_t meas_get( meas_result_t *out)
{
    /* Sección crítica de pocas instrucciones: la ISR de flancos no puede
     * modificar la medición mientras se lee y se evalúa su vigencia. */
    const uint32_t irq_state = save_and_disable_interrupts(); //impide que la isr de falncos imterrumpa
    //la medicion
    const uint32_t now = now_us(); 

    const bool idle = (uint32_t)(now - m.last_edge_t) > TIMEOUT_US; //verdadero si no hubo flancos en los
    //ultimos 500 ms
    if (idle || (m.pub_valid && (uint32_t)(now - m.pub_t) > TIMEOUT_US)) { // invalida el resultado
        //publicado si no hay flancos o si es demasiado viejo
        m.pub_valid = false;
    }
    meas_state_t st = idle ? MEAS_NO_TRANSITIONS //decide el estado ok, waiting, no_transition
                    : (m.pub_valid ? MEAS_OK : MEAS_WAITING);
    if (st == MEAS_OK) { //copia el resultado a quien llamos
        *out = m.pub;
    }

    restore_interrupts(irq_state); //restaura el estado de imterrupciones que habia antes.
    return st; //debuelve el estado
}
