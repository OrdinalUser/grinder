# Project

## References

- [github](https://github.com/vgg-fiit/ppgso-projekt-1600-mesto-tytykalo_studenc)
- [project registration - Mesto, 16:00](https://docs.google.com/spreadsheets/d/1dPwiYxHrZhV5wo3hsOPgEEXfqnwbwiTN2hg5crK-vGY)

## Deadlines

- do 26.10.2025 do 23:59 - [návrhu projektu](#návrhu-projektu---5b)
- do 09.12.2025 do 15:00 - [implementácia projektu](#implementácia-projektu---50b)
- do 14.12.2025 do 23:59 - [správa k projektu, prílohy](#správa-k-projektu-a-prílohy---5b)

## Requirements

### Grade

- (req) aspoň 3b z priebežného písomného testu
- (req) aspoň 35b v súčte z hodnotenia projektu, návrhu a správy k projektu, v súlade so stanovenými požiadavkami
- (req) Zo záverečnej skúšky je potrebné získať min. 10b
- (grade) Získanie aspoň 56b z celkového hodnotenia za predmet

### Technical

- at least 2 minutes
- demo must not be static
- should follow some kind of story and have a 'logical start', 'flow', 'logical end', and has to convey some kind of 'thought' -> demo has to be coherent

## Stages

### Návrhu projektu - 5b

- (3b) Náčrt jednotlivých scén a “príbehu” pomocou storyboard-u v čase (môže byť ručne kreslené)
- (2b) Podrobná špecifikácia, sumarizácia a opis správania sa objektov v scénach, opis grafických efektov

### Implementácia projektu - 50b

- [9b] 3D objekty
  - (2b) Objekty ako unikátne 3D polygonálne modely
  - (2b) Unikátne mapovanie textúr pomocou UV koordinátov na 3D objekt
  - (2b) Efektívne inštancovanie 3D objektu: zobrazenie aspoň 5000 inštancií za použitia OpenGL inštancovania
  - (2b) Priesvitné objekty
    - Správne vykreslenie priesvitných a nepriesvitných objektov v jednej scéne - napr. usporiadať priesvitné objekty a vykreslovať ich od najvzdialenejších po najbližšie ku kamere
  - (1b) Objekt s využitím hrbolatej textúry (normal mapping)

- [8b] Scéna
  - (2b) Procedurálne generovaná scéna
    - Obmedzenia a lokácia objektov definovaná (ne-)deterministickým algoritmom a/alebo na základe dátovej štruktúry
  - (4b) Hierarchická reprezentácia scény
    - Každá scéna bude mať logické priestorové usporiadanie (napr. podlaha, pozadie, obloha, strop, steny…) implementované pomocou grafu scény, ktorý bude implementovaný min. ako stromová dátová štruktúra
    - Využitie hierarchickej transformácie v scéne
    - Aspoň 2 levely hierarchie medzi 3-mi objektmi
    - Použitie kompozície maticových transformácii
  - (2b) Využitie techniky mapovania na kocku (cube mapping)
    - na vytvorenie tzv. Sky-box-u, alebo
    - na mapovanie okolitého prostredia na povrch objektu (tzv. Environment Mapping)

- [6b] Animované objekty
  - (2b) Procedurálna animácia
    - Simulácia komplexného správania/rozhodovania sa objektu (logické vetvenie, cyklus, uzavretá metóda s parametrami)
  - (3b) Animácia na základe dát uložených v kľúčových snímkoch a interpolácie
    - Kľúčový snímok je reprezentovaný samostatnou dátovou štruktúrou, ktorá uchováva transformácie objektu (min. pozícia a natočenie) v čase
  - (1b) Animácia riadená komplexnými animačnými krivkami pre plynulý pohyb (lineárna interpolácia a smoothstep funkcia nestačí)

- [7b] Simulácia, časticový systém
  - (3b) Efektívna kolízia medzi objektami
    - Implementovaná pomocou testov s tzv. ohraničujúcimi telesami
    - Dynamická odozva na kolíziu, t.j. správny odraz od nakloneného/šikmého povrchu podľa normály povrchu v mieste kolízie
  - (2b) Simulácia aspoň s dvoma silami s použitím vektorovej algebry
    - Musí sa počítať okrem rýchlosti aj zrýchlenie, napr. gravitácia + vietor, a musí podporovať hmotnosť telies
    - Správna implementácia numerickej integrácie, napr. semi-implicitná Eulerova integrácia, alebo RK4
  - (2b) Simulácia pevných telies (rigid bodies) so správný výpočtom síl, kombinácie lineárneho a uhlového pohybu, a aplikácie krútiaceho momentu

- [3b] Práca s kamerou
  - (1b) Kamera s perspektívnou projekciou, správna zmena veľkosti okna a zobrazovacieho priestoru
  - (2b) Animovaná kamera pomocou kľúčových snímkov, ktoré obsahujú informáciu v danom čase kde sa kamera nachádza a kam sa pozerá, použitie inej ako lineárnej interpolácie

- [13b] Osvetlenie za pomoci viacerých svetelných zdrojov
  - (3b) Použitie všetkých typov zdrojov svetla s príslušnými parametrami: smerové, bodové, reflektor
  - (1b) Zmena pozície a orientácie zdrojov svetla a odtieňov farby osvetlenia
  - (1b) Správne kombinovať difúzne svetlo z 2 zdrojov svetla s difúznymi materiálmi
  - (2b) Správny Phongov osvetlovací model s viacerými (aspoň 3) zdrojmi svetla, pričom musí byť splnené:
    - Správne tlmenie svetla na základe hĺbky
    - Použité aspoň tri zložky farby materiálu a tri zložky farby svetla
    - Správne kombinovať zložky farby materialu a zložky farby svetla
  - (1b) Implementácia Blinn-Phongovho modifikovaného osvetľovacieho modelu
  - (1b) HDR zobrazenie s použitím mapovania tónov (tone mapping) a gama korekcie
  - Tiene implementované
    - (1b) jednoduchým spôsobom, pričom tien sa musí prepočítavať vzhľadom na vzájomnú pozíciu a vzdialenosť objektu a zdroja svetla - tieň nesmie byt "staticky" prilepeny k objektu
    - (4b) pomocou shadow-maps

- [4b] Post-processing
  - Zobrazenie scény s aplikovaným post-processing filtrom
    - (1b) základný per-pixel filter napr. grayscale
    - cez framebuffer
      - (2b) konvolučný filter
      - (4b) pokročilý filter, napr. bloom

### Správa k projektu a prílohy - 5b

- Finálna správa a prílohy sa odovzdávajú samostatne do príslušných miest odovzdania podľa inštrukcií

---

- (5b) Správa v .pdf s nasledujúcou štruktúrou
  - titulná strana - meno a priezvisko študentov, predmet, čas cvičení
  - návrh projektu odovzdaný počas semestra
  - min. 6x A4 - opis najzaujímavejších vlastností riešenia
    - (1b) Dátové štruktúry
    - (1b) Výpis zaujímavých algoritmov
    - (1b) Opis scén pomocou grafu scény, opis priestorových vzťahov
    - (1b) Opis objektov v scéne pomocou diagramu tried
  - min. 2x A4 s obrázkami
    - (1b) ideálne ako sekvencia obrázkov zodpovedajúca storyboard-u
- Povinné prílohy:
  - spustitelný balík v .zip
  - reprezentatívny obrázok z projektu v .jpg
  - video celého projektu v .mkv/.mp4 (ideálne codec H265/HEVC)
