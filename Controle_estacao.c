#include <stdio.h>
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include"hardware/clocks.h"
#include"hardware/pio.h"
#include "aht20.h"
#include "bmp280.h"
#include "ssd1306.h"
#include "font.h"
#include <math.h>
#include "lwip/tcp.h"
#include "animacoes_led.pio.h"

// Wi-Fi
#define WIFI_SSID "Paixao 2"
#define WIFI_PASS "25931959"

#define I2C_PORT i2c0               // i2c0 pinos 0 e 1, i2c1 pinos 2 e 3
#define I2C_SDA 0                   // 0 ou 2
#define I2C_SCL 1                   // 1 ou 3
#define SEA_LEVEL_PRESSURE 101325.0 // Press√£o ao n√≠vel do mar em Pa
// Display na I2C
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C

#define Botao_A 5

#define Led_Verde 11
#define Led_Vermelho 13

#define Buzzer 21

// Pinos dos LEDs e matriz
#define MATRIZ_LEDS 7       // Pino de controle da matriz de LEDs 5x5
#define NUM_PIXELS 25       // N˙mero total de LEDs na matriz (5x5)

// ================= VARI¡VEIS GLOBAIS DO SISTEMA =================
// Hardware PIO para controle da matriz de LEDs
PIO pio;                    // Controlador PIO
uint sm;                    // State Machine do PIO
ssd1306_t ssd;             // Estrutura do display OLED

// VariÔøΩveis globais do sistema
// Dados dos sensores (simulados ou reais)
float temperatura = 0;
float umidade = 0;
float pressao = 0;

// Limites m·ximos
float temp_max = 40.0;
float umid_max = 80.0;
float pres_max = 102000;

int Pagina = 1;
int Desenho_letra = 0;

char Letra_T, Letra_P, Letra_U;

// ================= ENUMERA«’ES E ESTRUTURAS =================
/**
 * EnumeraÁ„o para cores dos LEDs
 */
typedef enum {
    COR_VERMELHO,   // Estado: capacidade m·xima atingida
    COR_VERDE,      // Estado: funcionamento normal
    COR_AZUL,       // Estado: ambiente vazio
    COR_AMARELO,    // Estado: apenas uma vaga restante
    COR_PRETO       // Estado: LEDs apagados
} CorLED;

/**
 * Estrutura para controle de debounce dos botıes
 */
typedef struct {
    uint32_t last_time;     // Timestamp do ˙ltimo pressionamento
    bool last_state;        // Estado anterior do bot„o
} DebounceState;


// Fun√ß√£o para calcular a altitude a partir da press√£o atmosf√©rica
double calculate_altitude(double pressure)
{
    return 44330.0 * (1.0 - pow(pressure / SEA_LEVEL_PRESSURE, 0.1903));
}

// ================= PADR’ES DA MATRIZ DE LEDS =================
/**
 * Matriz com padrıes visuais para a matriz de LEDs 5x5
 * Cada array representa um padr„o diferente (seta/apagado)
 */
double padroes_led[3][25] = {
    // Letra T de temperatura
    {0, 0, 0, 0, 0, 
     0, 0, 1, 0, 0, 
     0, 0, 1, 0, 0, 
     0, 1, 1, 1, 0, 
     0, 0, 0, 0, 0},
    
    // Letra U de umidade
    {0, 0, 0, 0, 0, 
     0, 1, 1, 1, 0, 
     0, 1, 0, 1, 0, 
     0, 1, 0, 1, 0, 
     0, 0, 0, 0, 0},

    // Letra P de press„o
    {0, 0, 0, 0, 0, 
     0, 1, 0, 0, 0, 
     0, 1, 1, 1, 0, 
     0, 1, 1, 1, 0, 
     0, 0, 0, 0, 0}
};

// ================= FUN«’ES DE CONTROLE DA MATRIZ DE LEDS =================
/**
 * Converte valores RGBW em formato de cor para a matriz de LEDs
 * 
 * r Intensidade do vermelho (0.0 - 1.0)
 * g Intensidade do verde (0.0 - 1.0) 
 * b Intensidade do azul (0.0 - 1.0)
 * w Intensidade do branco (0.0 - 1.0)
 * return Valor de cor de 32 bits para o LED
 */
uint32_t matrix_rgb(double r, double g, double b, double w) {
    uint8_t red = (uint8_t)(r * 75);
    uint8_t green = (uint8_t)(g * 75);
    uint8_t blue = (uint8_t)(b * 75);
    uint8_t white = (uint8_t)(w * 75);
    return (green << 24) | (red << 16) | (blue << 8) | white;
}

/**
 * Atualiza toda a matriz de LEDs com o padr„o e cor especificados
 * 
 * r Intensidade do vermelho
 * g Intensidade do verde
 * b Intensidade do azul  
 * w Intensidade do branco
 */
void Desenho_matriz_leds(double r, double g, double b, double w) {
    for (int i = 0; i < NUM_PIXELS; i++) {
        double valor = padroes_led[Desenho_letra][i];
        uint32_t cor_led = (valor > 0) ? matrix_rgb(g, r, b, w) : matrix_rgb(0, 0, 0, 0);
        pio_sm_put_blocking(pio, sm, cor_led);
    }
}

/**
 * Atualiza a matriz de LEDs usando cor prÈ-definida
 * 
 * cor Cor do enum CorLED
 */
void Desenho_matriz_leds_cor(CorLED cor) {
    double r = 0, g = 0, b = 0, p = 0;
    switch (cor) {
        case COR_VERMELHO: r = 1.0; break;
        case COR_VERDE: g = 1.0; break;
        case COR_AZUL: b = 1.0; break;
        case COR_AMARELO: r = 1.0; g = 1.0; break;
        case COR_PRETO: p = 1.0; break;
    }
    Desenho_matriz_leds(g, r, b, p);
}

// ================= HANDLERS DE INTERRUP«√O =================
/**
 * Handler de interrupÁ„o GPIO para o bot„o de reset
 * Implementa debounce por hardware e libera sem·foro para reset
 */
void gpio_irq_handler(uint gpio, uint32_t events) {
    static uint32_t last_time = 0;
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    // Debouncing de 300ms (300000 microsegundos)
    if ((current_time - last_time) > 300000) {
        
        // Verifica se È o bot„o correto E se est· pressionado (LOW devido ao pull-up)
        if (gpio == Botao_A && !gpio_get(Botao_A)) {
            last_time = current_time;
            
            Pagina += 1;
            
            printf("Pagina Atualizada\n");

            if(Pagina > 4){
                Pagina = 1;
            }
        }
    }
}
// P·gina HTML servida pelo servidor
const char HTML_BODY[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Monitoramento Avancado</title>"
"<script src='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.9.1/chart.min.js'></script>"
"<style>"
"body{font-family:'Segoe UI',sans-serif;margin:0;padding:20px;background:#f5f5f5;}"
".container{max-width:1200px;margin:0 auto;background:white;padding:30px;border-radius:15px;box-shadow:0 4px 20px rgba(0,0,0,0.1);}"
"h1{text-align:center;color:#2c3e50;margin-bottom:30px;font-size:2.5em;}"
"h2{color:#34495e;border-bottom:3px solid #3498db;padding-bottom:10px;margin-top:40px;}"
"h3{color:#2c3e50;margin-bottom:15px;}"
".chart-container{margin:30px 0;padding:20px;background:#fafafa;border-radius:10px;border-left:5px solid #3498db;height:400px;position:relative;}"
".values-display{display:flex;justify-content:space-around;margin:20px 0;flex-wrap:wrap;}"
".value-card{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:20px;border-radius:15px;text-align:center;min-width:200px;margin:10px;box-shadow:0 8px 25px rgba(0,0,0,0.15);}"
".value-card h4{margin:0 0 10px 0;font-size:1.1em;opacity:0.9;}"
".value-large{font-size:2.2em;font-weight:bold;margin:5px 0;}"
".value-small{font-size:1.1em;opacity:0.8;margin:2px 0;}"
".controls-section{background:#ecf0f1;padding:25px;border-radius:10px;margin:20px 0;}"
".control-group{margin:15px 0;}"
".control-group label{display:inline-block;width:150px;font-weight:bold;color:#2c3e50;}"
"input[type='number']{padding:10px;border:2px solid #bdc3c7;border-radius:8px;width:120px;font-size:16px;margin:5px;}"
"input[type='number']:focus{border-color:#3498db;outline:none;}"
"button{background:linear-gradient(135deg,#3498db,#2980b9);color:white;border:none;padding:12px 25px;border-radius:8px;cursor:pointer;font-size:16px;font-weight:bold;margin:5px;transition:all 0.3s;}"
"button:hover{transform:translateY(-2px);box-shadow:0 5px 15px rgba(52,152,219,0.4);}"
".status{margin:10px 0;padding:10px;border-radius:5px;text-align:center;font-weight:bold;}"
".status.success{background:#d5edda;color:#155724;border:1px solid #c3e6cb;}"
".status.info{background:#d1ecf1;color:#0c5460;border:1px solid #bee5eb;}"
"canvas{max-width:100%;height:350px !important;display:block;}"
"@media (max-width:768px){.values-display{flex-direction:column;}.value-card{margin:10px auto;}}"
"</style>"
"</head><body>"
"<div class='container'>"
"<h1>Monitoramento Avancado de Sensores</h1>"

"<div class='values-display'>"
"<div class='value-card'><h4>Temperatura</h4><div id='temp-display'>--&nbsp;&deg;C</div></div>"
"<div class='value-card'><h4>Umidade</h4><div id='umid-display'>--%</div></div>"
"<div class='value-card'><h4>Pressao</h4><div id='pres-display'>-- kPa</div></div>"
"</div>"

"<h2>Graficos Historicos</h2>"

"<div class='chart-container'>"
"<h3>Temperatura (&nbsp;&deg;C)</h3>"
"<canvas id='graficoTemp'></canvas>"
"</div>"

"<div class='chart-container'>"
"<h3>Umidade (%)</h3>"
"<canvas id='graficoUmid'></canvas>"
"</div>"

"<div class='chart-container'>"
"<h3>Pressao (Pa)</h3>"
"<canvas id='graficoPres'></canvas>"
"</div>"

"<div class='controls-section'>"
"<h2>Configuracoes de Calibracao</h2>"

"<h3>Offsets de Calibracao</h3>"
"<div class='control-group'><label>Offset Temperatura:</label><input id='temp_offset' type='number' step='0.01' placeholder='0.00'>&nbsp;&deg;C</div>"
"<div class='control-group'><label>Offset Umidade:</label><input id='umid_offset' type='number' step='0.1' placeholder='0.0'>%</div>"
"<div class='control-group'><label>Offset Pressao:</label><input id='pres_offset' type='number' step='1' placeholder='0'>Pa</div>"
"<button onclick='enviarOffsets()'>Atualizar Offsets</button>"

"<h3>Limites Maximos de Alerta</h3>"
"<div class='control-group'><label>Temp. Maxima:</label><input id='temp_max' type='number' step='0.1' placeholder='35.0'>&nbsp;&deg;C</div>"
"<div class='control-group'><label>Umidade Maxima:</label><input id='umid_max' type='number' step='0.1' placeholder='80.0'>%</div>"
"<div class='control-group'><label>Pressao Maxima:</label><input id='pres_max' type='number' step='1' placeholder='102000'>Pa</div>"
"<button onclick='enviarLimites()'>Atualizar Limites</button>"

"<div id='status'></div>"
"</div>"

"<script>"
// Vari·veis globais
"let labels = [], tempData = [], umidData = [], presData = [];"
"let tempOrigData = [], umidOrigData = [], presOrigData = [];"
"let maxDataPoints = 50;"
"let graficoTemp, graficoUmid, graficoPres;"

// FunÁ„o para criar os gr·ficos
"function criarGraficos() {"
"if (typeof Chart === 'undefined') {"
"setTimeout(criarGraficos, 100);"
"return;"
"}"

"const ctxTemp = document.getElementById('graficoTemp').getContext('2d');"
"const ctxUmid = document.getElementById('graficoUmid').getContext('2d');"
"const ctxPres = document.getElementById('graficoPres').getContext('2d');"

"graficoTemp = new Chart(ctxTemp, {"
"type: 'line',"
"data: {"
"labels: labels,"
"datasets: [{"
"label: 'Temperatura com Offset',"
"data: tempData,"
"borderColor: '#e74c3c',"
"backgroundColor: 'rgba(231,76,60,0.1)',"
"fill: true"
"}, {"
"label: 'Temperatura Original',"
"data: tempOrigData,"
"borderColor: '#f39c12',"
"backgroundColor: 'transparent',"
"fill: false,"
"borderDash: [5, 5]"
"}]"
"},"
"options: {"
"responsive: true,"
"maintainAspectRatio: false,"
"scales: {"
"y: {"
"beginAtZero: false,"
"title: {display: true, text: 'Temperatura (∞C)'}"
"},"
"x: {title: {display: true, text: 'Tempo'}}"
"},"
"plugins: {legend: {position: 'top'}}"
"}"
"});"

"graficoUmid = new Chart(ctxUmid, {"
"type: 'line',"
"data: {"
"labels: labels,"
"datasets: [{"
"label: 'Umidade com Offset',"
"data: umidData,"
"borderColor: '#3498db',"
"backgroundColor: 'rgba(52,152,219,0.1)',"
"fill: true"
"}, {"
"label: 'Umidade Original',"
"data: umidOrigData,"
"borderColor: '#9b59b6',"
"backgroundColor: 'transparent',"
"fill: false,"
"borderDash: [5, 5]"
"}]"
"},"
"options: {"
"responsive: true,"
"maintainAspectRatio: false,"
"scales: {"
"y: {"
"beginAtZero: false,"
"title: {display: true, text: 'Umidade (%)'}"
"},"
"x: {title: {display: true, text: 'Tempo'}}"
"},"
"plugins: {legend: {position: 'top'}}"
"}"
"});"

"graficoPres = new Chart(ctxPres, {"
"type: 'line',"
"data: {"
"labels: labels,"
"datasets: [{"
"label: 'Pressao com Offset',"
"data: presData,"
"borderColor: '#27ae60',"
"backgroundColor: 'rgba(39,174,96,0.1)',"
"fill: true"
"}, {"
"label: 'Pressao Original',"
"data: presOrigData,"
"borderColor: '#e67e22',"
"backgroundColor: 'transparent',"
"fill: false,"
"borderDash: [5, 5]"
"}]"
"},"
"options: {"
"responsive: true,"
"maintainAspectRatio: false,"
"scales: {"
"y: {"
"beginAtZero: false,"
"title: {display: true, text: 'Pressao (Pa)'}"
"},"
"x: {title: {display: true, text: 'Tempo'}}"
"},"
"plugins: {legend: {position: 'top'}}"
"}"
"});"
"}"

// FunÁ„o para atualizar gr·ficos
"function atualizarGraficos(){"
"fetch('/dados').then(r=>r.json()).then(data=>{"
"if(labels.length >= maxDataPoints){"
"labels.shift(); tempData.shift(); umidData.shift(); presData.shift();"
"tempOrigData.shift(); umidOrigData.shift(); presOrigData.shift();"
"}"
"let agora = new Date().toLocaleTimeString();"
"labels.push(agora);"
"tempData.push(data.temp);"
"umidData.push(data.umid);"
"presData.push(data.pres);"
"tempOrigData.push(data.temp_orig);"
"umidOrigData.push(data.umid_orig);"
"presOrigData.push(data.pres_orig);"

// Atualizar displays de valores
"document.getElementById('temp-display').innerHTML = `<div class='value-large'>${data.temp.toFixed(2)}&nbsp;&deg;C</div><div class='value-small'>Original: ${data.temp_orig.toFixed(2)}&nbsp;&deg;C</div>`;"
"document.getElementById('umid-display').innerHTML = `<div class='value-large'>${data.umid.toFixed(1)}%</div><div class='value-small'>Original: ${data.umid_orig.toFixed(1)}%</div>`;"
"document.getElementById('pres-display').innerHTML = `<div class='value-large'>${(data.pres/1000).toFixed(2)} kPa</div><div class='value-small'>Original: ${(data.pres_orig/1000).toFixed(2)} kPa</div>`;"

"if(graficoTemp && graficoUmid && graficoPres){"
"graficoTemp.update();"
"graficoUmid.update();"
"graficoPres.update();"
"}"
"}).catch(err=>console.error('Erro ao buscar dados:',err));"
"}"

// FunÁıes para enviar dados
"function enviarLimites(){"
"let t = document.getElementById('temp_max').value;"
"let p = document.getElementById('pres_max').value;"
"let u = document.getElementById('umid_max').value;"
"if(!t || !p || !u){alert('Por favor, preencha todos os limites.'); return;}"
"fetch(`/limites?temp=${t}&pres=${p}&umid=${u}`).then(r=>r.text()).then(resp=>{"
"document.getElementById('status').innerHTML = `<div class='status success'>${resp}</div>`;"
"setTimeout(()=>document.getElementById('status').innerHTML='', 3000);"
"}).catch(err=>{"
"document.getElementById('status').innerHTML = `<div class='status error'>Erro ao atualizar limites</div>`;"
"setTimeout(()=>document.getElementById('status').innerHTML='', 3000);"
"});"
"}"

"function enviarOffsets(){"
"let t = document.getElementById('temp_offset').value || 0;"
"let p = document.getElementById('pres_offset').value || 0;"
"let u = document.getElementById('umid_offset').value || 0;"
"fetch(`/offsets?temp=${t}&pres=${p}&umid=${u}`).then(r=>r.text()).then(resp=>{"
"document.getElementById('status').innerHTML = `<div class='status success'>${resp}</div>`;"
"setTimeout(()=>document.getElementById('status').innerHTML='', 3000);"
"}).catch(err=>{"
"document.getElementById('status').innerHTML = `<div class='status error'>Erro ao atualizar offsets</div>`;"
"setTimeout(()=>document.getElementById('status').innerHTML='', 3000);"
"});"
"}"

"function buscarOffsets(){"
"fetch('/get_offsets').then(r=>r.json()).then(data=>{"
"document.getElementById('temp_offset').value = data.temp_offset.toFixed(2);"
"document.getElementById('pres_offset').value = data.pres_offset.toFixed(0);"
"document.getElementById('umid_offset').value = data.umid_offset.toFixed(1);"
"}).catch(err=>console.error('Erro ao buscar offsets:',err));"
"}"

// InicializaÁ„o quando a p·gina carrega
"window.onload = function() {"
"criarGraficos();" // ESTA … A LINHA QUE ESTAVA FALTANDO!
"buscarOffsets();"
"atualizarGraficos();"
"setInterval(atualizarGraficos, 2000);"
"};"

"</script>"
"</div></body></html>";

// Vari·veis para offsets de calibraÁ„o
float temp_offset = 0.0;
float umid_offset = 0.0;
float pres_offset = 0.0;

// Estrutura para manter o estado da resposta HTTP
struct http_state {
    char response[9700]; 
    size_t len;
    size_t sent;
};

// Callback quando parte da resposta foi enviada
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len) {
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}

// Recebe e trata as requisiÁıes
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs) {
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    hs->sent = 0;

    if (strncmp(req, "GET / ", 6) == 0 || strncmp(req, "GET /index.html", 15) == 0) {
        int content_length = strlen(HTML_BODY);
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "Cache-Control: no-cache\r\n"
            "\r\n"
            "%s",
            content_length, HTML_BODY);
    } 
    else if (strstr(req, "GET /dados")) {
        // Envia os dados atuais dos sensores (com e sem offset)
        char json[256];
        int json_len = snprintf(json, sizeof(json),
            "{\"temp\":%.2f,\"umid\":%.2f,\"pres\":%.2f,\"temp_orig\":%.2f,\"umid_orig\":%.2f,\"pres_orig\":%.2f}", 
            temperatura + temp_offset, umidade + umid_offset, pressao + pres_offset,
            temperatura, umidade, pressao);

        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s", json_len, json);
    }
    else if (strstr(req, "GET /offsets")) {
        char *temp_str = strstr(req, "temp=");
        char *pres_str = strstr(req, "pres=");
        char *umid_str = strstr(req, "umid=");

        if (temp_str) temp_offset = atof(temp_str + 5);
        if (pres_str) pres_offset = atof(pres_str + 5);
        if (umid_str) umid_offset = atof(umid_str + 5);

        const char *resp = "Offsets de calibraÁ„o atualizados com sucesso!";
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; charset=UTF-8\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s", (int)strlen(resp), resp);
    }
    else if (strstr(req, "GET /get_offsets")) {
        // Retorna os offsets atuais
        char json[128];
        int json_len = snprintf(json, sizeof(json),
            "{\"temp_offset\":%.2f,\"umid_offset\":%.2f,\"pres_offset\":%.2f}", 
            temp_offset, umid_offset, pres_offset);

        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s", json_len, json);
    }
    else if (strstr(req, "GET /limites")) {
        char *temp_str = strstr(req, "temp=");
        char *pres_str = strstr(req, "pres=");
        char *umid_str = strstr(req, "umid=");

        if (temp_str) temp_max = atof(temp_str + 5);
        if (pres_str) pres_max = atof(pres_str + 5);
        if (umid_str) umid_max = atof(umid_str + 5);

        const char *resp = "Limites de alerta atualizados com sucesso!";
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; charset=UTF-8\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s", (int)strlen(resp), resp);
    }
    else {
        const char *resp = "404 - P·gina n„o encontrada";
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain; charset=UTF-8\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            (int)strlen(resp), resp);
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);
    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    pbuf_free(p);
    return ERR_OK;
}

// Configura nova conex„o
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

// Inicializa servidor HTTP
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) return;
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) return;
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
}

void Alerta_Led_RGB(){

    gpio_init(Led_Verde);
    gpio_set_dir(Led_Verde, GPIO_OUT);

    gpio_init(Led_Vermelho);
    gpio_set_dir(Led_Vermelho, GPIO_OUT);

    if((temperatura > temp_max)||(umidade > umid_max)||(pressao > pres_max)){

        gpio_put(Led_Vermelho, true);
        gpio_put(Led_Verde, false);
    }else{
        gpio_put(Led_Verde, true);
        gpio_put(Led_Vermelho, false);
    }
}

void Alerta_Buzzer(){

    gpio_init(Buzzer);
    gpio_set_dir(Buzzer, GPIO_OUT);

    if((temperatura > temp_max)||(umidade > umid_max)||(pressao > pres_max)){

        gpio_put(Buzzer, 1); // Liga o buzzer
        sleep_us(200);           // 500 microssegundos ligado
        gpio_put(Buzzer, 0); // Desliga o buzzer
        sleep_us(50);           // 500 microssegundos desligado

    }else{
        gpio_put(Buzzer, false);
    }
}

void Desenho_Matriz_Leds(){

    if(temperatura > temp_max){
        Letra_T = COR_VERMELHO;
    } else{
        Letra_T = COR_VERDE;
    }

    if(umidade > umid_max){
        Letra_U = COR_VERMELHO;
    } else{
        Letra_U = COR_VERDE;
    }

    if(pressao > pres_max){
        Letra_P = COR_VERMELHO;
    } else{
        Letra_P = COR_VERDE;
    }

    switch (Desenho_letra)
    {
    case 0:
        Desenho_letra = 0;  // Padr„o apagado
        Desenho_matriz_leds_cor(Letra_T);
        break;
    
    case 1:
        Desenho_letra = 1;  // Padr„o apagado
        Desenho_matriz_leds_cor(Letra_U);
        break;

    case 2:
        Desenho_letra = 2;  // Padr„o apagado
        Desenho_matriz_leds_cor(Letra_P);
        break;
    
    default:
        break;
    }
    
    if(Desenho_letra > 2){
        Desenho_letra = 0;
    }else{
        Desenho_letra += 1;
    }
}

int main()
{
    // Para ser utilizado o modo BOOTSEL com bot√£o B
    gpio_init(Botao_A);
    gpio_set_dir(Botao_A, GPIO_IN);
    gpio_pull_up(Botao_A);
    gpio_set_irq_enabled_with_callback(Botao_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
   
    // InicializaÁ„o PIO para controle da matriz
    pio = pio0;
    uint offset = pio_add_program(pio, &animacoes_led_program);
    sm = pio_claim_unused_sm(pio, true);
    animacoes_led_program_init(pio, sm, offset, MATRIZ_LEDS);

   
    stdio_init_all();

    // I2C do Display funcionando em 400Khz.
    i2c_init(I2C_PORT_DISP, 400 * 1000);

    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA_DISP);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL_DISP);                                        // Pull up the clock line
    ssd1306_t ssd;                                                     // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT_DISP); // Inicializa o display
    ssd1306_config(&ssd);                                              // Configura o display
    ssd1306_send_data(&ssd);                                           // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicializa o I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa o BMP280
    bmp280_init(I2C_PORT);
    struct bmp280_calib_param params;
    bmp280_get_calib_params(I2C_PORT, &params);

    // Inicializa o AHT20
    aht20_reset(I2C_PORT);
    aht20_init(I2C_PORT);

    // Estrutura para armazenar os dados do sensor
    AHT20_Data data;
    int32_t raw_temp_bmp;
    int32_t raw_pressure;

    char str_temp_med[5];  // Buffer para armazenar a string
    char str_press[5];  // Buffer para armazenar a string  
    char str_tmp2[5];  // Buffer para armazenar a string
    char str_umi[5];  // Buffer para armazenar a string 
    
    char str_Temp_Max[5];  // Buffer para armazenar a string
    char str_Umid_Max[5];  // Buffer para armazenar a string  
    char str_Pres_Max[5];  // Buffer para armazenar a string

    char str_Temp_OFFSET[5];  // Buffer para armazenar a string
    char str_Umid_OFFSET[5];  // Buffer para armazenar a string  
    char str_Pres_OFFSET[5];  // Buffer para armazenar a string

    char IP[15];  // Buffer para armazenar a string  
    char REDE[15];  // Buffer para armazenar a string

    bool cor = true;

    if (cyw43_arch_init()) return 1;
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 5000)) {
        ssd1306_fill(&ssd, 0);
        ssd1306_draw_string(&ssd, "Falha Wi-Fi", 0, 0);
        ssd1306_send_data(&ssd);
        return 1;
    }

    uint32_t ip_addr = cyw43_state.netif[0].ip_addr.addr;
    uint8_t *ip = (uint8_t *)&ip_addr;
    char ip_display[24];
    snprintf(ip_display, sizeof(ip_display), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    ssd1306_fill(&ssd, 0);
    ssd1306_draw_string(&ssd, "Wi-Fi => OK", 0, 0);
    ssd1306_draw_string(&ssd, ip_display, 0, 10);
    ssd1306_send_data(&ssd);

    start_http_server();

    while (1)
    {
        cyw43_arch_poll();

        Alerta_Led_RGB();

        Alerta_Buzzer();

        Desenho_Matriz_Leds();

        // Leitura do BMP280
        bmp280_read_raw(I2C_PORT, &raw_temp_bmp, &raw_pressure);
        int32_t temperature_bmp = bmp280_convert_temp(raw_temp_bmp, &params);
        int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);

        // C√°lculo da altitude
        double altitude = calculate_altitude(pressure);

        printf("Pressao = %.3f kPa\n", pressure / 1000.0);
        printf("Temperatura BMP: = %.2f C\n", temperature_bmp / 100.0);
        printf("Altitude estimada: %.2f m\n", altitude);

        printf("IP: %s\n", ip_display);

        // Leitura do AHT20
        if (aht20_read(I2C_PORT, &data))
        {
            printf("Temperatura AHT: %.2f C\n", data.temperature);
            printf("Umidade: %.2f %%\n\n\n", data.humidity);
        }
        else
        {
            printf("Erro na leitura do AHT10!\n\n\n");
        }

        temperatura = ((((temperature_bmp/100) + data.temperature)/2) + temp_offset);
        pressao = ((bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params)) + pres_offset);
        umidade = (data.humidity + umid_offset);

        sprintf(str_temp_med, "%.1fC", temperatura);  // Converte o inteiro em string
        sprintf(str_press, "%.1fkPa", pressao/1000);  // Converte o inteiro em string
        sprintf(str_umi, "%.0f%%", umidade);  // Converte o inteiro em string 
        
        sprintf(str_Temp_Max, "%.1fC", temp_max);  // Converte o inteiro em string
        sprintf(str_Umid_Max, "%.0f%%", umid_max);  // Converte o inteiro em string
        sprintf(str_Pres_Max, "%.1fKPa", pres_max/1000);  // Converte o inteiro em string

        sprintf(str_Temp_OFFSET, "%.1fC", temp_offset);  // Converte o inteiro em string
        sprintf(str_Umid_OFFSET, "%.0f%%", umid_offset);  // Converte o inteiro em string
        sprintf(str_Pres_OFFSET, "%.1fKPa", pres_offset/1000);  // Converte o inteiro em string

        uint32_t ip_addr = cyw43_state.netif[0].ip_addr.addr;
        uint8_t *ip = (uint8_t *)&ip_addr;
        char ip_display[24];
        snprintf(ip_display, sizeof(ip_display), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    
        //  Atualiza o conte√∫do do display com anima√ß√µes
        if(Pagina == 1){
        ssd1306_fill(&ssd, !cor);                           // Limpa o display
        ssd1306_rect(&ssd, 1, 1, 122, 60, cor, !cor);       // Desenha um ret√¢ngulo
        ssd1306_line(&ssd, 1, 25, 123, 25, cor);            // Desenha uma linha
        ssd1306_line(&ssd, 1, 37, 123, 37, cor);            // Desenha uma linha
        ssd1306_line(&ssd, 1, 48, 123, 48, cor);            // Desenha uma linha
        ssd1306_draw_string(&ssd, "ESTACAO", 35, 6);  // Desenha uma string
        ssd1306_draw_string(&ssd, "METEROLOGICA", 17, 16);   // Desenha uma string
        ssd1306_draw_string(&ssd, "TEMP MED:", 3, 29); // Desenha uma string
        ssd1306_draw_string(&ssd, "UMIDADE:", 3, 40); // Desenha uma string
        ssd1306_draw_string(&ssd, "PRESSAO:", 3, 51); // Desenha uma string
        ssd1306_draw_string(&ssd, str_temp_med, 75, 29);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_press, 66, 51);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_umi, 73, 40);            // Desenha uma string
        } 
        else if (Pagina == 2){
        ssd1306_fill(&ssd, !cor);                           // Limpa o display
        ssd1306_rect(&ssd, 1, 1, 122, 60, cor, !cor);       // Desenha um ret√¢ngulo
        ssd1306_line(&ssd, 1, 25, 123, 25, cor);            // Desenha uma linha
        ssd1306_line(&ssd, 1, 37, 123, 37, cor);            // Desenha uma linha
        ssd1306_line(&ssd, 1, 49, 123, 49, cor);            // Desenha uma linha
        ssd1306_draw_string(&ssd, "LIMITE MAXIMO", 12, 12);  // Desenha uma string
        ssd1306_draw_string(&ssd, "TEMP MAX ", 3, 29); // Desenha uma string
        ssd1306_draw_string(&ssd, "UMID MAX ", 3, 40); // Desenha uma string
        ssd1306_draw_string(&ssd, "PRES MAX ", 3, 51); // Desenha uma string
        ssd1306_line(&ssd, 70, 25, 70, 60, cor);            // Desenha uma linha vertical
        ssd1306_draw_string(&ssd, str_Temp_Max, 73, 29);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_Umid_Max, 73, 41);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_Pres_Max, 73, 51);            // Desenha uma string
        } 
        else if (Pagina == 3){
        ssd1306_fill(&ssd, !cor);                           // Limpa o display
        ssd1306_rect(&ssd, 1, 1, 122, 60, cor, !cor);       // Desenha um ret√¢ngulo
        ssd1306_line(&ssd, 1, 25, 123, 25, cor);            // Desenha uma linha
        ssd1306_line(&ssd, 1, 37, 123, 37, cor);            // Desenha uma linha
        ssd1306_line(&ssd, 1, 49, 123, 49, cor);            // Desenha uma linha
        ssd1306_draw_string(&ssd, "OFFSET SENSORES", 3, 12);  // Desenha uma string
        ssd1306_draw_string(&ssd, "T OFFSET", 3, 29); // Desenha uma string
        ssd1306_draw_string(&ssd, "U OFFSET", 3, 40); // Desenha uma string
        ssd1306_draw_string(&ssd, "P OFFSET", 3, 51); // Desenha uma string
        ssd1306_line(&ssd, 70, 25, 70, 60, cor);            // Desenha uma linha vertical
        ssd1306_draw_string(&ssd, str_Temp_OFFSET, 73, 29);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_Umid_OFFSET, 73, 41);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_Pres_OFFSET, 73, 51);            // Desenha uma string
        }
        else if (Pagina == 4){
        ssd1306_fill(&ssd, !cor);                           // Limpa o display
        ssd1306_rect(&ssd, 1, 1, 122, 50, cor, !cor);       // Desenha um ret√¢ngulo
        ssd1306_line(&ssd, 1, 25, 123, 25, cor);            // Desenha uma linha
        ssd1306_line(&ssd, 1, 38, 123, 38, cor);            // Desenha uma linha
        ssd1306_draw_string(&ssd, "CONEXAO WI-FI", 8, 12);  // Desenha uma string
        ssd1306_draw_string(&ssd, "IP ", 3, 29); // Desenha uma string
        ssd1306_draw_string(&ssd, "CONEXAO OK", 20, 41); // Desenha uma string
        ssd1306_line(&ssd, 32, 25, 32, 38, cor);            // Desenha uma linha vertical
        ssd1306_draw_string(&ssd, ip_display, 34, 29);             // Desenha uma string
        
        }
        ssd1306_send_data(&ssd);                            // Atualiza o display

        sleep_ms(500);
    }

    return 0;
}