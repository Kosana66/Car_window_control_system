# **Sistem za kontrolu prozora u automobilu**
## **Opis projekta**
Cilj projekta je softver za automatsku ili manuelnu kontrolu prozora u automobilu. Korisnik moze izabrati rezim rada, granicnu brzinu nakon koje se prozor podizu, nivo prozora koji ce se odrzavati kao i manuelno upravljanje prozorima i zakljucavanjem zadnjih prozora. Takodje, omogucen je prikaz podataka o sistemu na displeju i prikaz poruka kao odgovora na komande.
## **Zahtevi projekta**
- [x] minimum 4 taska
	- task za prijem podataka od senzora prozora i senzora brzine
	- task za slanje podataka od PC-ja 
	- task za prijem podataka od PC-ja  
	- task za obradu podataka
	- task za prikaz na displeju
- [x] taskovi koji sluze za primanje i slanje podataka napraviti sto jednostavnijim, samo da pokupe podatak i proslede ga taskovima koji obradjuju podatke

- [x] sihronizacija izmedju tajmera i taskova, kao i izmedju taskova ako je potrebno, realizovati pomocu semafora ili mutexa
- [x] podatke izmedju taskova slati preko redova (queue)

- [x] trenutne podatke od senzora simulirati pomocu simulatora serijske komunikacije svakih 200 ms na kanalu 0 (iskoristiti mogucnost automatskog odgovora pomocu Triger-a)

- [x] za simulaciju logickih ulaza i izlaza koristiti LED bar
- [x] kada se vrsi automatska kontrola, podrazumeva se da se nakon odredjene brzine automobila prozor ne sme drzati otvoren i salje se signal da se prozori zatvore, kada se brzina spusti ispod propisane, prozori se vracaju na prethodno stanje 

- [x] pratiti podatke sa senzora za sva 4 prozora kao i brzine automobila i posmatrati vrednosti koje se dobijaju iz UniCom softvera sa kanala 0, onda usrednjiti zadnjih 10 odbiraka vrednosti sa senzora brzine, kako bi se dobio realniji podatak o prosecnoj brzini
- [x] ako je trenutni rezim rada sistema AUTOMATSKI, podrazumeva se da se prozori kontrolisu prema senzoru brzine. U tu svrhu potrebno je pratiti srednju vrednost brzine. Zavisno od vrednosti brzine postaviti prozore u trazeno stanje ili u skroz podignuto stanje.

- [x] komunikacija sa PC-jem ostvariti isto preko simulatora serijske veze, ali na kanalu 1
- [x] naredbe i poruke koje se salju preko serijske veze treba da sadrze samo ascii slova i brojeve, i trebaju se zavrsavati sa carriage return (CR), tj brojem 13 (decimalno), cime se detektuje kraj poruke 
- [x] naredbe su:
	- Komande rada prozora MANUELNO i AUTOMATSKI, kojima se menja rezima rada prozora, sistem treba da odgovori sa OK.
	- Pomocu komande NIVO<broj> potrebno je zadati nivo prozora koji se očekuje da se dostigne u procentima (0-skroz spuaten, 100-skroz podignut).
	- Pomocu komande BRZINA<broj> potrebno je zadati maksimalnu brzinu pri kojoj prozori smeju biti otvoreni gde je broj vrednost u kilometrima na cas, na primer 130. 
	- Slanje ka PC-iju (UniCom kanal 1) trenutne vrednost stanja prozora i brzine, rezim rada prozora. Kontinualno slati podatke periodom od 5000ms.

- [x] manuelna kontrola se resava pomocu dugmića iz LED Bar programa
- [x] ako je trenutni rezim rada MANUELNO, podrazumeva se da se nivo prozora reguliše preko prekidaca. Zavisno koja je ulazna LED ukljucena tada se podize prozor i ukljucuje odg. izlazna LED dioda. Dok se prozor podize ili spusta, slati poruku putem serijske komunikacije sa informacijama da li se prozor podize i spusta i koji prozor je u pitanju. Dodati jednu LED diodu kao prekidac koji zakljucava zadnje prozore. Ukoliko je LED ukljucena, zadnji prozori se moraju podici do kraja (ukoliko nisu podignuti), i na svaki pokusaj spustanja prozora, salje se poruka sa obavjestenjem da nije moguce izvrsiti zeljenu operaciju.

- [x] za simulaciju displeja koristiti Seg7Mux
- [x] na LCD displeju prikazati trenutnu vrednost brzine i režim rada (AUTOMATSKI rezim sa brojem 0, a MANUELNI sa 1), osvežavanje podataka 1000ms. Pored toga pamtiti minimalnu i maksimalnu izmerenu vrednost brzine od ukljucivanja sistema, i zavisno od toga koji je ulazni “taster” pritisnut na LED baru (predvideti tastere za to), prikazati minimalnu ili maksimalnu vrednost pored trenutne vrednosti brzine.

- [x] Misra pravila. Kod koji vi budete pisali mora poštovati MISRA pravila, ako je to moguće (ako nije dodati komentar zašto to nije moguće). Nije potrebno ispravljati kod koji niste pisali, tj. sve one biblioteke FreeRTOSa i simulatora hardvera.
- [x] obavezno je projekat postaviti na GitHub i na gitu je obavezno da ima barem jedan issue i pull request. Takodje obavezan je Readme.md fajl kao i .gitignore fajl sa odgovarajućim sadrzajima. Navesti u Readme fajlu kako testirati projekat.
## **Pokretanje programa**
- Preuzeti ceo repozitorijum sa Github sajta. Neophodan je instaliran Visual Studio 2019 kao okruzenje u kom se radi i PC lint kao program za staticku analizu koda.
- Otvoriti Visual Studio, kliknuti na komandu Open a local folder i otvoriti preuzeti repozitorijum. Zatim, sa desne strane kliknuti na FreeRTOS_simulator_final.sln, a onda na foldere Starter i Source Files i na fajl main.application kako bi se prikazao kod u radnom prostoru.
- U gornjem srednjem delu ispod Analyze podesiti x86 i pokrenuti softver pomocu opcije Local Windows Debugger koja se nalazi pored.
## **Pokretanje periferija**
- Preko Command Prompt-a uci u folder Periferije. 
- U folderu AdvUniCom pokrenuti tri puta aplikaciju AdvUniCom i kao argument svaki put proslediti po jedan broj (0,1,2) da bi se otvorio odg. kanal. 
- Zatim u folderu LEDbars pokrenuti aplikaciju LED_bars_plus i kao argument proslediti bG.
- Zatim u folderu 7SegMux pokrenuti aplikaciju Seg7_Mux i kao argument proslediti broj 7.
- Sada ste spremni za testiranje softvera.
## **Upustvo za testiranje softvera**
- U COM0 umesto T1 uneti S i oznaciti Auto 1, a zatim umesto R1 uneti simulirane podatke sa senzora(nivo prednjeg levog, prednjeg desnog, zadnjeg levog i zadnjeg desnog prozora i trenutnu brzinu) koji se moraju zavrsiti sa CR(\0d). Nivoi kao i brzina moraju biti trocifreni, dok je nivo prozora u opsegu od 0 do 100%(npr. 027086078098145\0d ). 
Nakon unesenih podataka kliknuti ok1 sa desne strane i oznaciti TBE INT u donjem desnom cosku. Tada sistem prima podatke sa senzora na svakih 200ms, a na svakih 5s ispisuje nivo prednjeg levog, prednjeg desnog, zadnjeg levog i zadnjeg desnog prozora, trenutnu brzinu i rezim rada na kanalu 1( 027 086 078 098 145 a ).
- U startu je ogranicenje brzine 100km/h i sistem je u automatskom rezimu, tako da ce nakon nekoliko desetina sekundi ispisivati da su svi prozori podignuti ( 100 100 100 100 145 a ).
- U COM1 u drugo polje od gore se unose komande : MANUELNO\0d, AUTOMATSKI\0d, NIVOxyz\0d, BRZINAxyz\0d.
- Unosom komande AUTOMATSKI, na COM1 se ispisuje OK i prebacuje se rezim u automatski (iako je pri pokretanju softvera podeseno da bude automatski).
- Slanjem BRZINA200\0d, postavljamo ogranicenje na 200km/h pa ce se nivo prozora vratiti na prethodnu vrednost(onu koja se salje sa senzora, COM1 -> 027 086 078 098 145 a ). 
- Slanjem NIVO015\0d nivo svih prozora je postavljen na 15%, pa na kanalu 1 se ispisuje 015 015 015 015 145 a . Taj nivo ce biti postavljen sve dok se ne posalje komanda NIVO sa nekim drugim brojem ili komanda AUTOMATSKI. 
- Nakon komande AUTOMATSKI ponovo se koriste podaci sa senzora i nivo se regulise u zavisnosti od ogranicenja brzine.
- U slucaju da se unese nivo veci od 100% ili da se komanda posalje dok je srednja brzina veca od ogranicenja, komanda ce se ignorisati.
- Sada cemo umesto 145 kao brzinu na COM0 poslati 250 (027086078098250\0d). 
- Sve vreme se na 7seg displejima ispisuje rezim rada(0 za automatski), trenutna brzina (prvo je bilo 145, a onda 250) i minimalna brzina (145) jer je podeseno da se prikazuje minimalna brzina pri pokretanju softvera.
- Slanjem MANUELNO\0d sistem se prebacuje u manuelni rezim i odgovara sa OK na COM1, a na prvom 7seg displeju se prikazuje 1 za manuelni rezim rada.
- Pritiskom na levi klik poslednje 4 diode prvog stuba redom, pale se poslednje 4 diode drugog stuba, svi prozori se redom podizu do 100% i na COM2 se ispisuju redom poruke risePL, risePZ, riseZL, riseZD. Analogno tome, desnim klikom se spustaju svi prozori na 0%, gase se te diode drugog stuba i ispisuju poruke sa fall umesto rise.
- Cetvrta dioda od gore prvog stuba podize zadnje prozore do 100%, zakljucava ih i pali poslednje dve diode drugog stuba. Tada nakon svakog pokusaja da se spusti neki od zadnjih prozora, na COM2 se ispisuje impossible. 
- Treca dioda od gore prvog stuba omogucava da na poslednja 3 7seg displeja se prikazuje minimalna brzina(145), dok druga dioda od gore omogucava prikaz maksimalne brzine sistema(250).




