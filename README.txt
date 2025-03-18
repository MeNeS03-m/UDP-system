Kandidatnr. 15094
Det er vel forklaring på alt i .h filer som vi alle måtte følge så forklaringen blir veldig lik som i 
de filene, men prøver å få det til litt mer unikt. -Alt er også kommentert i selve koden, som 
skal gi ganske bra forklaring på hva som skjer i de forskjellige funksjonene 
Implementasjonen som er beskrevet, integrerer et pålitelig UDP kommunikasjonssystem for å håndtere 
data overføring og bekreftelse mellom en klient og en server. Her ser du hvordan hver komponent og 
funksjon i systemet fungerer. Hver funksjon er robust mot potensielle errors.

1.	Oppretting av en UDP client(d1_create_client)
-	Minne er allokert for en D1Peers-struktut som samler klientens informasjon.
-	En UDP socket lages og initialiseres. Hvis opprettelsen av socket mislykkes, vil det tildelte 
minnet frigjøres og funksjonen vil returnere NULL.
-	Klientens adress structure er satt til null og konfigurert for IPv4.

2.	Slette en UDP client (d1_delete)
-	Først og fremst så sjekker funksjonen om den medfølgende D1-Peer-pointeren er gyldig.
-	Den assosierte socketen blir stengt-closed og alt allokert minne for D1Peer strukturen blir 
frigjort.

3.	Angi peer-info
-	Funksjonen initialiserer klientens adresse og setter portnummeret.
-	Videre prøver den å konvertere adressen fra et dotted decimal format og løser det via DNS 
om nødvendig, og fyller ut IP adressen i klientens adressestruktur.

4.	Motta data(d1_recv_data)
-	Data mottas fra serveren. Hvis mottaksoperasjonen er ikke blokkerende og ingen data er 
tilgjengelig, returnerer funksjonen en spesifikk error. 
-	Størreslen på pakken kontrolleres for å sikre at den oppfyller kravene for D1Header
-	Checksummen av mottat data blir validert for å sikre dataintegritet. Hvis checksummen er 
mislykkes, blir en bekreftelse med neste seqnr sendt.
-	Payloaden blir trukket ut av pakken, og funksjonen returnerer størrelsen på payload.
-	Skjønner ikke helt hvorfor, men når jeg prøvde å bruke checksum funskjonen her, så ville det 
absolutt ikke fungere.

5.	Vente på en bekreftelse (d1_wait_ack)
-	Funksjonen setter en timeout for mottaksoperasjonen for å forhindre blokkering.
-	Den venter for en bekreftelse-pakke, ackpakke som samsvarer forventet seqnr.
-	Håndterer eventuelle mottaksfeil på en grei måte.

6.	Sende data(d1_send_data)
-	Funksjonen sjekker størrelsen på dataene som skal sendes, og sikrer at den ikke overskrider 
bufferstørrelsen på 1024.
-	En pakke blir satt sammen, inkludert både headeren og dataen. Pakkens checksum blir 
beregnet og satt i headeren. 
-	Pakken blir videre send til den angitte adressen. Etter dette, venter funksjonen på en 
bekreftelse ved hjelp av d1_wait_ack, og sikrer at dataen ble mottatt riktig før den fortsetter.

7.	Sende bekreftelse – ACK(d1_send_ack)
-	En ACK pakke blir forberedt og blir sendt til peer. Pakken inneholder seqnr og kalkulert 
checkum for å forsikre dataintegritet.

8.	Checksum hjelpefunksjon
-	Kalkulerer checksum ved en XOR operasjon på data pakken for å sikre dataintegritet, og for 
at testene skal passere.

D2
1.	Oprette en klient (d2_client_create) 
- Denne funksjonen initialiserer en D2client-struktur, som inkluderer å sette opp en UDP klient 
(D1Peer) for nettverkskomunikasjon.
-	Klienten er konfigurert ved å bruke det angitte servernavnet og porten som involverer DNS 
oppløsning om nødvendig. Feil-errors på et hvilket som helst trinn resulterer av minne 
frigjøring – opprydding av ressurser og returnering av NULL. (minneallokering, peer eller 
DNS)

2.	Slette en klient(d2_client_delete)
-	Deallokerer D2Client strukturen på en sikker måte, og sikrer at alle tilknyttende ressurser er 
riktig utgitt (UDP peer). Dette forhindrer minne lekkasjer ved å sikre at alt dynamisk tildelt 
minne blir frigjort.

3.	Sending av request(d2_send_request)
-	Lager en PacketRequest med spesifisert ID, sender den gjennom nettet og håndterer 
potensielle feil i overføringen. Den funksjonen er avgjørende for å starte «transactions» som 
krever henting eller endring av dataen på serveren.

4.	Motta Response size(d2_recv_response_size)
-	Denne mottar det første svaret fra serveren, som indikerer hvor mange NetNode-strukturer 
(som representerer trenoder) som vil følge i påfølgende meldinger. Denne funskjonen 
analyserer svaret for å trekke ut og validere pakkens integritet og type før behandling.

5.	Motta svar (d2_recv_response)
-	Denne håndterer mottak av payload fra serveren som inneholder faktisk data. Denne 
forsikrer at dataen som blir mottatt er riktig og at den er riktig mottatt. Logger eventuelle 
problemer i tillegg.

6.	Allokering av Local Tree (d2_alloc_local_tree)
-	Skal oprette dynamisk lagring for en lokal representasjon av trestrukturen. Denne funksjonen 
allokerer en array av netnode pointers, slik at klienten kan lagre og se hierarkisk data mottatt 
fra serveren.

7.	Frigjøring av lokalt tre(d2_free_local_tree)
-	Frigjør alt minne knyttet til det lokale treet.

8.	Legge til noder i det lokale treet(d2_add_to_local_tree)
- 	Funksjoner sjekker først for null pointers og en ikke negativ bufferlengde, og sikrer at argumentene som ble sendt inn 
	er riktige. Når denne iterer vil den Lese id, verdi og num_children feltene fra bufferen, og koverterer dem fra nettverksbyte rekkefølge
	til host byte order ved bruk av ntohl().
	For hvert barne-id en node leser funksjonen ID-en fra bufferen og legger den til child-id matrisen i noden.
	Funksjonen sikrer at den ikke overskrider forhåndstildelt plass for noder i treet.

9.	Skrive ut treet(d2_print_tree)
-	Sender ut hele trestrukturren til client terminalen. Denne funksjonen får iterativt tilgang til 
hver node i det lokale treet og skriver ut detajer som node id og verdi.

10. Skrive ut treet rekursivt(print_tree_recursive)
-	Denne funksjonen skriver ut treet på en formatert måte rekursivt, og viser den hierarkiske strukturen til treet. 
	Den sjekker om node_index er in bound,. Den øker intrykk(tab) skriver ut id, verdi og antall underordnede til gjeldene node.
	For hvert barn til den gjeldende noden kaller funksjonen seg selv rekursivt for å skrive ut undertreet.


Alt skal være velfungerende og outputene er som forventet.
Alt er implementert.
Jeg har noen få warnings når koden blir kompilert, men det er for unused parameter og slikt, så tror ikke det skal være så stress

