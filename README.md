# Tema 3 PCOM - HTTP Client

*timp rezolvare: aprox 4 zile*

## Descriere

In cadrul acestei teme am ales sa imi scriu implementarea in limbajul **C++** gratie bilbiotecii **STL**. De asemenea am modularizat functionalitatile programului cu ajutorul mai multor clase care interactioneaza intre ele, cu mentiunea ca ele sunt scrise in acelasi fisier, nu separat.

## Prezentare tehnica

### Clase folosite

- TCP --- face posibila conectarea la server si pune la dispozitie o *interfata* prin care trimit si se primesc mesaje TCP
    - ```start()``` --- deschiderea unui socket pentru comunicare
    - ```stop()``` --- inchiderea socket-ului
    - ```send_message()``` --- trimiterea unui mesaj TCP (functia se asigura ca tot mesajul a fost transmis)
    - ```recv_message()``` --- primirea unui mesaj; prin parsarea datelor primite se determina dimensiunea body-ului (daca este cazul) si este receptat tot mesajul
- HTTP --- se ocupa cu generarea de **cereri HTTP** pentru request-urile: **GET, POST, DELETE, PUT**, luand in considerare *cookie-urile*, *token jwt*, calea, parametrii url si datele json.
- HTTP_Res --- primeste raspunsul HTTP de la clasa TCP si il interpreteaza
    - ```add_response()``` --- adauga noi date pentru a le procesa
    - ```check_response()``` --- verifica daca raspunsul are un cod de succes de tip **2XX**, sau de eroare (orice cod care nu este de succes)
    - ```get_data()``` --- extrage *body-ul* din raspuns
    - ```get_error()``` --- extrage mesajul de eroare
    - ```get_json()``` --- returneaza datele json din *body*
- Client --- gestioneaza interactiunea cu utilizatorul: preluarea comenzilor si executarea functiilor in conformitate cu **API-ul** serverului si cu comenzile disponibile pentru client

### Json

In ceea ce priveste lucrul cu structurile de tip **json** am decis (asa cum este si recomandat in textul temei de casa) sa folosesc o biblioteca deja disponibila, astfel am ajuns la **nlohmann/json** (disponibila aici: https://github.com/nlohmann/json). Utilizarea acestei biblioteci este rezultatul unui singur fapt: daca nu as fi folosit-o ar fi trebuit sa implementez manual o clasa de parsare json, astfel dubland timpul de rezolvare. Cat despre locurile in care este utilizata: la preluarea datelor de la utilizator si construirea cererii, cat si la parsarea raspunsului de la server.

### Alte mentiuni

- Input-ul de la utilizator este verificat mereu, fie pentru corectitudinea datelor introduse, fie pentru introducerea unei comenzi valide.
