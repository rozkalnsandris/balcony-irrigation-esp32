# Aparatūra un pinout

## Zināmais sastāvs

- ESP32 Dev Module
- 15 augsnes mitruma sensori
- CD74HC4067 16-kanālu analogais multipleksors
- R385 ūdens sūknis
- active-low releja modulis

## ESP32 pinout no pašreizējā firmware

| Funkcija | GPIO | Piezīme |
|---|---:|---|
| Relay | 26 | active-low |
| MUX S0 | 13 | select bit 0 |
| MUX S1 | 12 | **pašlaik kodā un vēsturē fiziski izmantots**; GPIO12/MTDI ir ESP32 strapping pin, tāpēc gala montāžā paredzēta pārcelšana uz GPIO25 |
| MUX S2 | 14 | select bit 2 |
| MUX S3 | 27 | select bit 3 |
| MUX SIG | 34 | analog input |

## Mitruma kalibrācijas atskaites punkti

| Stāvoklis | RAW ADC aptuveni |
|---|---:|
| galīgi sauss | 2217 |
| vajag laistīt | 1850 |
| mitrs | 1175 |

Firmware sliekšņi:
- `> 2000` → `sauss`
- `< 1400` → `mitrs`
- starp tiem → `videjs`

## Nepārbaudītā/atvērtā aparatūras daļa

Claude vēsturē kā pareizā fail-safe topoloģija ir dokumentēts releja **NO** kontakts, lai bez vadības sūknis paliktu OFF. Fiziskais vadojums vēl jāapstiprina uz reālās iekārtas.

Motora induktīvais trieciens/back-EMF ir atvērts risks. Pirms to atzīmēt kā atrisinātu, fiziski jāpārbauda flyback diode un atbilstoša barošanas decoupling/kondensatoru aizsardzība, pēc tam jāveic atkārtoti sūkņa cikli.

## Kāpēc GPIO12 jāpārvieto

ESP32 GPIO12 ir arī `MTDI` strapping pin. Reset/power-on brīdī tā līmenis var izvēlēties `VDD_SDIO` flash barošanas spriegumu. Uz tipiska 3.3 V flash ESP32 nepareizi augsts GPIO12 reset laikā var radīt boot/flashing problēmas. Tāpēc MUX select līnijai drošāka gala izvēle ir GPIO25, pēc fiziskā vada pārlikšanas atjauninot arī `MUX_S1` firmware.
