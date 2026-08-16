# Aparatūra un pinout

## Zināmais sastāvs

- ESP32 Dev Module
- 15 augsnes mitruma sensori
- CD74HC4067 16-kanālu analogais multipleksors
- R385 DC ūdens sūknis; vēsturiskajos incidenta materiālos aprakstīts kā 12 V motors
- active-low releja modulis
- vēsturē uzstādīta motora/back-EMF aizsardzība: `1N5408 + 100nF + 470µF`

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

## Releja fail-safe topoloģija

Vecajā Claude/LinkedIn draftā bija kļūdains `NC` apgalvojums. Vēlākā koriģētā vēsture skaidri nosaka **NO (Normally Open)** kā pareizo fail-safe OFF topoloģiju: bez vadības/releja aktivizācijas sūknim jāpaliek OFF.

Koriģētajā materiālā vienlaikus bija “VERIFY REAL HARDWARE”, tāpēc dokumentācijas statuss ir:

- `NO` = pareizais projekta dizains;
- exact pašreizējais `COM/NO/NC` spaiļu vadojums vēl jānofotografē/nostiprina kā fizisks pierādījums, īpaši pēc jebkādas pārlikšanas.

## Back-EMF / motora trokšņu vēsture

Agrāk ESP32 restartējās tieši sūkņa **izslēgšanas** brīdī. Vēsturiskā diagnostika sasaistīja to ar DC motora induktīvo kickback/back-EMF un barošanas traucējumiem.

Vēsturē dokumentētais labojums:

- `1N5408` flyback diode pāri sūkņa termināļiem;
- `100 nF` keramiskais kondensators;
- `470 µF` elektrolītiskais/bulk kondensators barošanas sliedes stabilizēšanai.

Atgūtajā post-fix testa ierakstā sūknis darbojās apmēram 15 s un pēc OFF ESP32 nepazuda. Jauni sensoru unavailable stāvokļi pumpja laikā neparādījās; viens sensors bija unavailable jau pirms testa. Tas ir **historical PASS** vienam īsam ciklam.

Tāpēc šo komponentu uzstādīšana vairs nav “neizdarīts TODO”. Atvērts paliek tikai **pašreizējās montāžas fizisks foto/reinspection**, ja sistēma kopš tā laika ir pārlikta, un plašāka ciklu stabilitātes pārbaude tikai ar atsevišķu owner autorizāciju.

## Kāpēc GPIO12 jāpārvieto

ESP32 GPIO12 ir arī `MTDI` strapping pin. Reset/power-on brīdī tā līmenis var izvēlēties `VDD_SDIO` flash barošanas spriegumu. Uz tipiska 3.3 V flash ESP32 nepareizi augsts GPIO12 reset laikā var radīt boot/flashing problēmas. Tāpēc MUX select līnijai drošāka gala izvēle ir GPIO25, pēc fiziskā vada pārlikšanas atjauninot arī `MUX_S1` firmware.

## Vēsturiskās sensoru piezīmes

Claude-era testos dažādos brīžos bija redzami `Puķe 8` un `puke_5` unavailable gadījumi. Tie nav current hardware status. Pirms sensora remonta vispirms jāpārbauda svaigs `/mitrums` un `/raw` rezultāts.

Plašāka provenance un pretrunu tabula: [`HISTORICAL_KNOWLEDGE_BASE.md`](HISTORICAL_KNOWLEDGE_BASE.md).