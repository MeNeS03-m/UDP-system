Kandidatnr. 15094
Det er vel forklaring p� alt i .h filer som vi alle m�tte f�lge s� forklaringen blir veldig lik som i 
de filene, men pr�ver � f� det til litt mer unikt. -Alt er ogs� kommentert i selve koden, som 
skal gi ganske bra forklaring p� hva som skjer i de forskjellige funksjonene 
Implementasjonen som er beskrevet, integrerer et p�litelig UDP kommunikasjonssystem for � h�ndtere 
data overf�ring og bekreftelse mellom en klient og en server. Her ser du hvordan hver komponent og 
funksjon i systemet fungerer. Hver funksjon er robust mot potensielle errors.

1.	Oppretting av en UDP client(d1_create_client)
-	Minne er allokert for en D1Peers-struktut som samler klientens informasjon.
-	En UDP socket lages og initialiseres. Hvis opprettelsen av socket mislykkes, vil det tildelte 
minnet frigj�res og funksjonen vil returnere NULL.
-	Klientens adress structure er satt til null og konfigurert for IPv4.

2.	Slette en UDP client (d1_delete)
-	F�rst og fremst s� sjekker funksjonen om den medf�lgende D1-Peer-pointeren er gyldig.
-	Den assosierte socketen blir stengt-closed og alt allokert minne for D1Peer strukturen blir 
frigjort.

3.	Angi peer-info
-	Funksjonen initialiserer klientens adresse og setter portnummeret.
-	Videre pr�ver den � konvertere adressen fra et dotted decimal format og l�ser det via DNS 
om n�dvendig, og fyller ut IP adressen i klientens adressestruktur.

4.	Motta data(d1_recv_data)
-	Data mottas fra serveren. Hvis mottaksoperasjonen er ikke blokkerende og ingen data er 
tilgjengelig, returnerer funksjonen en spesifikk error. 
-	St�rreslen p� pakken kontrolleres for � sikre at den oppfyller kravene for D1Header
-	Checksummen av mottat data blir validert for � sikre dataintegritet. Hvis checksummen er 
mislykkes, blir en bekreftelse med neste seqnr sendt.
-	Payloaden blir trukket ut av pakken, og funksjonen returnerer st�rrelsen p� payload.
-	Skj�nner ikke helt hvorfor, men n�r jeg pr�vde � bruke checksum funskjonen her, s� ville det 
absolutt ikke fungere.

5.	Vente p� en bekreftelse (d1_wait_ack)
-	Funksjonen setter en timeout for mottaksoperasjonen for � forhindre blokkering.
-	Den venter for en bekreftelse-pakke, ackpakke som samsvarer forventet seqnr.
-	H�ndterer eventuelle mottaksfeil p� en grei m�te.

6.	Sende data(d1_send_data)
-	Funksjonen sjekker st�rrelsen p� dataene som skal sendes, og sikrer at den ikke overskrider 
bufferst�rrelsen p� 1024.
-	En pakke blir satt sammen, inkludert b�de headeren og dataen. Pakkens checksum blir 
beregnet og satt i headeren. 
-	Pakken blir videre send til den angitte adressen. Etter dette, venter funksjonen p� en 
bekreftelse ved hjelp av d1_wait_ack, og sikrer at dataen ble mottatt riktig f�r den fortsetter.

7.	Sende bekreftelse � ACK(d1_send_ack)
-	En ACK pakke blir forberedt og blir sendt til peer. Pakken inneholder seqnr og kalkulert 
checkum for � forsikre dataintegritet.

8.	Checksum hjelpefunksjon
-	Kalkulerer checksum ved en XOR operasjon p� data pakken for � sikre dataintegritet, og for 
at testene skal passere.

D2
1.	Oprette en klient (d2_client_create) 
- Denne funksjonen initialiserer en D2client-struktur, som inkluderer � sette opp en UDP klient 
(D1Peer) for nettverkskomunikasjon.
-	Klienten er konfigurert ved � bruke det angitte servernavnet og porten som involverer DNS 
oppl�sning om n�dvendig. Feil-errors p� et hvilket som helst trinn resulterer av minne 
frigj�ring � opprydding av ressurser og returnering av NULL. (minneallokering, peer eller 
DNS)

2.	Slette en klient(d2_client_delete)
-	Deallokerer D2Client strukturen p� en sikker m�te, og sikrer at alle tilknyttende ressurser er 
riktig utgitt (UDP peer). Dette forhindrer minne lekkasjer ved � sikre at alt dynamisk tildelt 
minne blir frigjort.

3.	Sending av request(d2_send_request)
-	Lager en PacketRequest med spesifisert ID, sender den gjennom nettet og h�ndterer 
potensielle feil i overf�ringen. Den funksjonen er avgj�rende for � starte �transactions� som 
krever henting eller endring av dataen p� serveren.

4.	Motta Response size(d2_recv_response_size)
-	Denne mottar det f�rste svaret fra serveren, som indikerer hvor mange NetNode-strukturer 
(som representerer trenoder) som vil f�lge i p�f�lgende meldinger. Denne funskjonen 
analyserer svaret for � trekke ut og validere pakkens integritet og type f�r behandling.

5.	Motta svar (d2_recv_response)
-	Denne h�ndterer mottak av payload fra serveren som inneholder faktisk data. Denne 
forsikrer at dataen som blir mottatt er riktig og at den er riktig mottatt. Logger eventuelle 
problemer i tillegg.

6.	Allokering av Local Tree (d2_alloc_local_tree)
-	Skal oprette dynamisk lagring for en lokal representasjon av trestrukturen. Denne funksjonen 
allokerer en array av netnode pointers, slik at klienten kan lagre og se hierarkisk data mottatt 
fra serveren.

7.	Frigj�ring av lokalt tre(d2_free_local_tree)
-	Frigj�r alt minne knyttet til det lokale treet.

8.	Legge til noder i det lokale treet(d2_add_to_local_tree)
- 	Funksjoner sjekker f�rst for null pointers og en ikke negativ bufferlengde, og sikrer at argumentene som ble sendt inn 
	er riktige. N�r denne iterer vil den Lese id, verdi og num_children feltene fra bufferen, og koverterer dem fra nettverksbyte rekkef�lge
	til host byte order ved bruk av ntohl().
	For hvert barne-id en node leser funksjonen ID-en fra bufferen og legger den til child-id matrisen i noden.
	Funksjonen sikrer at den ikke overskrider forh�ndstildelt plass for noder i treet.

9.	Skrive ut treet(d2_print_tree)
-	Sender ut hele trestrukturren til client terminalen. Denne funksjonen f�r iterativt tilgang til 
hver node i det lokale treet og skriver ut detajer som node id og verdi.

10. Skrive ut treet rekursivt(print_tree_recursive)
-	Denne funksjonen skriver ut treet p� en formatert m�te rekursivt, og viser den hierarkiske strukturen til treet. 
	Den sjekker om node_index er in bound,. Den �ker intrykk(tab) skriver ut id, verdi og antall underordnede til gjeldene node.
	For hvert barn til den gjeldende noden kaller funksjonen seg selv rekursivt for � skrive ut undertreet.


Alt skal v�re velfungerende og outputene er som forventet.
Alt er implementert.
Jeg har noen f� warnings n�r koden blir kompilert, men det er for unused parameter og slikt, s� tror ikke det skal v�re s� stress

