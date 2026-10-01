# Schlüsselwort-Analyse: Was aus dem Protokoll in welchen Bogen gehört

Grundlage: Beispiel 1 (Gesprächsprotokoll, Erhebungsbogen C, ausgefüllter Basisbogen A,
Gesundheitsbogen B, GP-Vorbereitung). Die Sprecherkürzel im Protokoll:

| Kürzel | Wer (Beispiel 1) | Woran erkennbar |
|---|---|---|
| ohne Kürzel, Ich-Form | Frau Anna Apfelkuchen, leistungsberechtigte Person | „Ich fahre mit dem Fahrrad …“ |
| FS | Frau S., Pflegemutter (Betreutes Wohnen in Familien) | „meine 3 Kinder“, „Im Dezember hast du hier angefangen“; GP: „Familie S.“, „betreutes Wohnen in Familien“ |
| FF | Frau F., Begleitung außerhalb der Werkstatt (Funktion nicht im Protokoll) | „Die ärztliche Begleitung teilen wir uns“; GP: „von Frau F. oder Frau S. begleitet“ |
| FD | Fachdienst der Werkstatt (Liebenau) | „Wir haben ein offenes Konzept mit mehreren Arbeitsbereichen“ |
| SD | Sozialer Dienst, Landratsamt (bedarfsermittelnde Fachkraft) | „SD: Was sind die Aufgaben in der Werkstatt?“ |

Die Funktionen von FS und FF sind aus den Dokumenten erschlossen, nicht ausdrücklich
genannt. Deshalb steht im Prompt eine Legende, die die Mitarbeitende pro Fall ausfüllt.

## 1. Grobverteilung: Signal im Protokoll → Bogen

| Signalwörter im Protokoll | Fundstelle Beispiel 1 (wörtlich) | Zielbogen und Feld |
|---|---|---|
| Name der Person, „Frau …“ | „Anna Apfelkuchen war sehr fit“ (FS) | A 1.1 Name; A 1.1 Geschlecht (abgeleitet) |
| „hier“, „hierher“, Einrichtungsname, Ortsname | „Danach bin ich hierhergekommen (Liebenau)“ | A 2.1 Ort (abgeleitet) |
| Sprecherkürzel FS/FF/FD/SD | alle Zeilen mit Kürzel | A 2.1 Teilnehmende Personen mit Funktion und Kürzel |
| Betreuer/in, Betreuerwechsel, Vollmacht, Beschluss, Amtsgericht | „Es gab jetzt ein Betreuerinnenwechsel. Zu Frau Koch … mit Frau Holzmann“ (FS) | A 1.3 Rechtliche Vertretung (nur abgeleitet, prüfen) |
| Dolmetscher, Gebärden, Leichte Sprache, Talker, „versteht nicht“ | kein Treffer; Person antwortet selbst | A 2.2 Kommunikationshilfen: nicht erforderlich (abgeleitet) |
| letzter Teilhabebericht, letzter GP, Zielüberprüfung, „Siehe Vorbereitungsbogen“ | „Aus dem Teilhabebericht vom 13.02.26“ (Erhebungsbogen) | A 3.1 Folgeermittlung |
| zugeschickt, Kopie, „möchte die Bögen“ | kein Treffer | A 3.3 Versand: nein + Standardsatz |
| GdB, Merkzeichen, Ausweis, Pflegegrad, Pflegekasse, Versorgungsamt | kein Treffer | A 4 (OFFEN, aus der Akte) |
| Diagnose, Krankheit, Bandscheibe, OP, Reha, MRT, Ärzte, Arztwechsel, Gewicht, Schmerz | „dreifachen Bandscheibenvorfall“ (FS), „OP-Vorbesprechungstermin in Biberach“ (FS), „Gewicht zu verlieren“ (FS) | B Ergänzende Hinweise; C Lebensbereich 5; **nicht** A 4 |
| Konzentration, Antrieb, Motivation, „Impulse von außen“, „aus der Reserve locken“ | „Wie es ist mit der längeren Konzentration ist noch fraglich“; „Es braucht aber bisher noch Impulse von außen“ (FD) | B Körperfunktionen (Erläuterung b130/b140); C LB 2 und LB 7 |
| sitzen, stehen, Belastbarkeit, „am Stück“ | „2 Stunden am Stück sitzen ist schwierig, nur stehen ist auch schwierig“ (FS) | B Ergänzende Hinweise; C LB 8; GP LB 4/8 |
| Fahrrad, Bus, Bushaltestelle, fahren, Treppe, Schnee, km | „es sind 5KM bis zur Bushaltestelle in Bad Wurzach“ (FS); „muss eine Treppe im Haus steigen“ (FF) | C LB 4; GP LB 4 |
| einkaufen, kochen, Wäsche, putzen, Fertigsuppe, backen | „Wäsche waschen, putzen und so weiter mache ich alles selber.“ | C LB 6; GP LB 6 |
| Arzttermine selbst/begleitet, essen mit, frühstücken | „Anna Apfelkuchen macht den ein oder anderen Termin auch selber.“ (FF) | C LB 5; GP LB 5 |
| Werkstatt, Innenbereich, Aufträge, Arbeitszeiten, Küchenteam, Arbeitsmarkt, Fernziel | „Ich bin im Innenbereich und arbeite gerne mit Fahrradtaschen.“ | C LB 8; GP LB 8 und Wünsche |
| Kontakt, „verstehe mich mit“, zugehörig, Kollegen, spielt mit Kindern | „Ich verstehe mich mit Alexa ganz gut“ | C LB 7; GP LB 7 |
| Musik, Paillettenbilder, Urlaub, Freizeit, Konzert, Aktion Mensch | „In der Pause höre ich Musik“ | C LB 9; GP LB 9 |
| wohne, Haus, abgetrennter Wohnbereich | „Ich wohne mit der Familie im gleichen Haus, habe aber einen abgetrennten Wohnbereich.“ | C Lebenssituation Wohnen; GP Lebensform |
| wünscht sich, Fernziel, möchte, „kann ich mir vorstellen“, „überlegs mir“, „nächstes Jahr“ | „hat das Fernziel auf dem ersten Arbeitsmarkt zu arbeiten“ (FS) | GP Wünsche und Ziele; C Wünsche |
| kann, genau, gewissenhaft, fit, pünktlich | „Du kannst viel gut, arbeitest genau und gewissenhaft.“ (FD) | GP Kompetenzen und Ressourcen |

Faustregel: **Der Basisbogen ist ein Verwaltungsbogen.** Aus dem Gespräch kommen nur
Beteiligte, Ort, ein Hinweis auf rechtliche Betreuung, Kommunikationshilfen und
der Versandwunsch. Alles Übrige (Geburtsdatum, Adresse, GdB, Pflegegrad, Art der
wesentlichen Behinderung, Aufgabenkreise der Betreuung) kommt aus der Akte.

## 2. Basisbogen A: Feld für Feld

| Feld | Typ | Signalwörter | Beispiel 1: Fund | Falle |
|---|---|---|---|---|
| 1.1 Name, Vorname | Text | vollständiger Name, „Frau/Herr“ | „Frau Anna Apfelkuchen“ (Erhebungsbogen) → Apfelkuchen, Anna | Spitznamen, nur Vorname |
| 1.1 Geburtsdatum | Text | Datum, „geboren“, Alter | kein Fund → OFFEN | Alter ist kein Geburtsdatum |
| 1.1 Geschlecht | 3 Kästchen | „Frau/Herr“, „sie/er“ | „Frau“, „sie“ → weiblich (ABGELEITET) | – |
| 1.1 Familienstand | 4 Kästchen | Ehefrau/-mann, verheiratet, geschieden, verwitwet, Partner | kein Fund → OFFEN | „ledig“ nicht vermuten, auch wenn niemand erwähnt wird |
| 1.1 Anzahl Kinder | Zahl | „mein Sohn / meine Tochter / meine Kinder“ **in Ich-Form der Person** | kein Fund → OFFEN | „meine 3 Kinder“ sagt FS, nicht die Person |
| 1.1 Staatsangehörigkeit | 2 Kästchen + Text | Staatsangehörigkeit, Pass, Aufenthalt | kein Fund → OFFEN | „deutsch“ nicht vermuten |
| 1.2 Kontaktdaten | 4 Textfelder | Straße, PLZ, Telefonnummer, Mailadresse | kein Fund → OFFEN | „wohnen sehr außerhalb“, „5 km zur Bushaltestelle in Bad Wurzach“ ist keine Adresse |
| 1.3 Rechtliche Vertretung | 4 + 3 Kästchen | Betreuer/in, gesetzliche/rechtliche Betreuung, Betreuungsverein, Vollmacht, Beschluss | „Betreuerinnenwechsel … Frau Koch … Frau Holzmann“ (FS) → rechtliche Betreuung (ABGELEITET, prüfen) | „Betreuerin“ kann Wohn- oder Werkstattbetreuung meinen |
| 1.3 Aufgabenbereiche | 6 Kästchen | Vermögenssorge, Gesundheitsfürsorge, Aufenthaltsbestimmung, Behörden, Wohnung, Post | kein Fund → OFFEN | nie aus dem Gespräch ableiten, nur Betreuerausweis/Beschluss |
| 1.4 Personen des Vertrauens | Text | Vertrauensperson, Bezugsperson, „meine Ansprechpartnerin“, von der Person selbst benannt | kein Fund → OFFEN, Hinweis: FS kommt in Frage | Begleitperson ≠ Vertrauensperson |
| 2.1 Datum, Ort | Text | Datum im Protokollkopf; „hier“, Einrichtungsname | „hierhergekommen (Liebenau)“, „Hier her fahre ich mit dem Bus“ → Ort Werkstatt der Liebenau (ABGELEITET); Datum OFFEN | – |
| 2.1 Teilnehmende Personen | Text | alle Kürzel + Person | FS, FF, FD, SD + Frau Apfelkuchen | Funktionen nur aus der Legende |
| 2.2 Kommunikationshilfen | 2 Kästchen | Dolmetscher, Gebärden, Leichte Sprache, Talker, Schriftdolmetscher | kein Signal, Person antwortet selbst → nicht erforderlich (ABGELEITET) | – |
| 3.1 Erst-/Folgeermittlung | 2 Kästchen | letzter Teilhabebericht, letzter GP, letzte Bedarfsermittlung, Zielüberprüfung / Erstantrag, erstmalig | „Letzter Teilhabebericht für weitere Informationen beachten“, „Aus dem Teilhabebericht vom 13.02.26“ → Folgeermittlung (ABGELEITET) | – |
| 3.1 Daten der Ermittlungen | Text | Datum + „abgeschlossen“ | kein Fund → OFFEN | – |
| 3.2 Fachkraft | Text | aus Legende SD | Legende ohne Namen → OFFEN | – |
| 3.3 Versand | 2 Kästchen + Text | zugeschickt, Kopie, per Post, „möchte die Bögen haben“ | kein Fund → nein (ABGELEITET) + Standardsatz „Der Wunsch zum Erhalt des BEI BW wurde im Gespräch nicht geäußert.“ | – |
| 4.1 Schwerbehinderung | 3 Kästchen + Text | GdB, Grad der Behinderung, Schwerbehindertenausweis, Merkzeichen G/B/H/aG/RF, Versorgungsamt, beantragt | kein Fund → OFFEN | Krankheit ≠ GdB |
| 4.2 Wesentliche Behinderung | 3 + 4 Kästchen | „wesentliche Behinderung“, „festgestellt“, Diagnose mit Zuordnung | kein Fund → OFFEN | **Bandscheibenvorfall führt nicht zu „körperliche“**; im Beispiel ist nur „geistige“ angekreuzt (Diagnose F70.0 aus Bogen B) |
| 4.3 Pflegegrad | 8 Kästchen | Pflegegrad, Pflegekasse, Pflegegeld, Medizinischer Dienst/MDK | kein Fund → OFFEN | – |
| 5 Ergänzende Hinweise | Text | Rest, der in keinen anderen Bogen gehört | nichts → `---` | Gesundheitsangaben gehören in Bogen B |

## 3. Vorschau: Signale für die nächsten beiden Prompts

**Gesundheitsbogen B** (nur die Anteile aus dem Gespräch):

| Feld | Signalwörter | Beispiel 1 |
|---|---|---|
| Ergänzende Hinweise | Krankheit, Diagnose, OP, Reha, MRT, Schmerz, Gewicht, Arzt | „dreifachen Bandscheibenvorfall“, „OP-Vorbesprechungstermin in Biberach“, „konsequent dabei Gewicht zu verlieren“ |
| b130 Antrieb (Erläuterung) | Motivation, Antrieb, „Impulse von außen“, „aus der Reserve locken“, „nicht so motiviert“ | „da war sie aber noch nicht so motiviert“ (FS); „Manchmal muss man sie aus der Reserve locken“ (FS) |
| b140 Aufmerksamkeit (Erläuterung) | Konzentration, Ausdauer, „am Stück“ | „Wie es ist mit der längeren Konzentration ist noch fraglich“ |
| b164 Höhere kognitive Funktionen (Erläuterung) | aussuchen, entscheiden, planen, „fällt schwer“ | „Man kann immer aussuchen, was man machen will, das fällt die aber noch schwer.“ (FD) |
| Kapitel 7 Bewegung | sitzen, stehen, Treppe, Rücken | „2 Stunden am Stück sitzen ist schwierig“ (FS); „muss eine Treppe im Haus steigen“ (FF) |

**GP-Vorbereitung** (Lebensbereiche 1–9 wie Erhebungsbogen C, dazu):

| Feld | Signalwörter | Beispiel 1 |
|---|---|---|
| Wünsche: Arbeiten/Lernen | Fernziel, möchte, wünscht sich, Arbeitsmarkt | „hat das Fernziel auf dem ersten Arbeitsmarkt zu arbeiten, z.B. im Getränkemarkt“ (FS) |
| Wünsche: Zeit | nächstes Jahr, Freizeit, ausprobieren | „Nächstes Jahr greifen wir voll an mit Freizeitaktivitäten.“ (FS) |
| Lebensform | wohne, Haus, Familie, abgetrennt | „Ich wohne mit der Familie im gleichen Haus, habe aber einen abgetrennten Wohnbereich.“ |
| Kompetenzen und Ressourcen | kann, genau, gewissenhaft, selber, Fahrrad | „Du kannst viel gut, arbeitest genau und gewissenhaft.“ (FD); „Wäsche waschen, putzen und so weiter mache ich alles selber.“ |
| Neue Ziele / Maßnahmen | 1x/Woche, Option, umstrukturieren, „überlegs mir“ | „Z.B. 1x/Woche in den Außenbereich.“ (FS); „ABM Wlaking zwischen 13-13:45“ (FD) |
| Krankenhausbegleitung | Krankenhaus, OP, Begleitung | „OP-Vorbesprechungstermin in Biberach“ (FS) → Frage stellen, nicht ableiten |

## 4. Erhebungsbogen C: Lebensbereich für Lebensbereich

Grundlage: die Zuordnung der Fachkraft in Beispiel 1. Der vollständige Katalog mit Fallen
steht im Prompt `05_prompt_erhebungsbogen_c.txt`.

| Block | Signalwörter | Beispiel 1 (wörtlich, Sprecher) |
|---|---|---|
| Wünsche | wünscht sich, möchte, will, Fernziel, „kann ich mir vorstellen“, nächstes Jahr, „wir haben den Wunsch“ | „hat das Fernziel auf dem ersten Arbeitsmarkt zu arbeiten“ (FS); „Nächstes Jahr greifen wir voll an mit Freizeitaktivitäten.“ (FS) |
| Lebenssituation | Ich-Aussagen ohne Kürzel: „Ich wohne“, „Ich bin im“, „Ich war in“, „In der Pause“ | „Ich wohne mit der Familie im gleichen Haus, habe aber einen abgetrennten Wohnbereich.“ |
| LB 1 Lernen | lernen, üben, sich aneignen, Motivation für Neues | „da war sie aber noch nicht so motiviert“ (FS) |
| LB 2 Aufgaben | Tagesablauf, Arbeitszeiten, Pausen, Konzentration bei Aufgaben, Zeitfenster, Pflichten | „Halb 9 fange ich an, um 10 hab ich Pause“; „In der Zeit gehe ich mit Anna Apfelkuchen einkaufen.“ (FS) |
| LB 3 Kommunikation | sprechen, verstehen, Telefon, Gebärden, Sprache | kein Fund → „Keine Einschränkungen ersichtlich.“ |
| LB 4 Mobilität | gehen, Treppe, Fahrrad als Verkehrsmittel, Bus, Bushaltestelle, km, Schnee, fahren | „es sind 5KM bis zur Bushaltestelle in Bad Wurzach“ (FS); „muss eine Treppe im Haus steigen“ (FF) |
| LB 5 Selbstversorgung | Ärzte, Arzttermine, OP, Reha, MRT, Medikamente, Gesundheit achten | „Jetzt haben wir einen OP-Vorbesprechungstermin in Biberach.“ (FS) |
| LB 6 Häusliches Leben | einkaufen, kochen, backen, Fertigsuppe, Wäsche, putzen, Haushalt | „Wäsche waschen, putzen und so weiter mache ich alles selber.“ |
| LB 7 Beziehungen | Kontakt, „verstehe mich mit“, zugehörig, Impulse von außen, Betreuerin | „Ich verstehe mich mit Alexa ganz gut“ |
| LB 8 Bedeutende Lebensbereiche | Werkstatt, Arbeitsbereich, Aufträge, Arbeitsleistung, Belastbarkeit bei der Arbeit, Fernziel Arbeitsmarkt, Gesundheit im Arbeitskontext | „Du kannst viel gut, arbeitest genau und gewissenhaft.“ (FD); „2 Stunden am Stück sitzen ist schwierig“ (FS) |
| LB 9 Gemeinschaft | Freizeit, Musik, basteln, Urlaub, Konzert, Angebote, Fahrrad als Fitness, mit Kindern spielen | „auf eine Freizeit in Italien mit der Werkstatt“ (FF); „Da kann ich alle Lieder auswendig.“ |
| Umweltfaktoren | Personen und Dinge, die helfen oder hindern: Pflegefamilie, Bushaltestelle, Schnee, Treppe, Werkstattkonzept | Förderfaktor: Pflegemutter; Barriere: ländliche Wohnlage im Winter |

Drei Zuordnungsregeln, die in Beispiel 1 den Unterschied machen:

1. **Gesundheit im Arbeitskontext gehört zu LB 8, ärztliche Behandlung zu LB 5.** „Das hat
   sie bei der Arbeit auf dem Hof körperlich stark beeinflusst“ → LB 8; „OP-Vorbesprechungstermin“ → LB 5.
2. **Fahrrad ist zweideutig.** Als Verkehrsmittel („Ich fahre mit dem Fahrrad nach Wurzach zur
   Arbeit“) LB 4, als Fitness und Freizeit („sie ist viel Fahrrad gefahren“) LB 9.
3. **Alltag anderer Sprecher zählt nur, wenn er den Alltag der Person strukturiert.** Die
   Kindergarten- und Schulzeiten der Pflegekinder stehen in LB 2, weil FS in dieser Zeit mit
   der Person einkaufen geht.

## 5. Gesundheitsbogen B: Signalwörter je ICF-Code

Grundregel: Aus dem Gespräch kommen nur Erläuterungen und Ergänzende Hinweise. Diagnosen
und Kreuze brauchen eine medizinische Quelle. In Beispiel 1 stammen Diagnose F70.0 und die
sechs Kreuze (b117, b122, b130, b140, b144, b164) aus der Akte; das Gespräch liefert dazu
Erläuterungen.

| Code | Signalwörter im Gespräch | Beispiel 1 (wörtlich, Sprecher) |
|---|---|---|
| b117 Intelligenz | versteht komplexe Texte nicht, einfache Sprache nötig | kein Fund im Protokoll (Referenz: Kreuz aus Akte) |
| b122 Globale psychosoziale Funktionen | zugehörig, „Impulse von außen“, sich einlassen, „nicht so offen“ | „Es braucht aber bisher noch Impulse von außen.“ (FD); „da ist sie bisher aber nicht so offen“ (FF) |
| b130 Antrieb | Motivation, „nicht so motiviert“, „aus der Reserve locken“ | „da war sie aber noch nicht so motiviert“ (FS); „Manchmal muss man sie aus der Reserve locken“ (FS) |
| b140 Aufmerksamkeit | Konzentration, „am Stück“, Ausdauer | „Wie es ist mit der längeren Konzentration ist noch fraglich“ |
| b144 Gedächtnis | vergisst, erinnert werden | kein Fund (Stärke „alle Lieder auswendig“ zählt nicht) |
| b164 Höhere kognitive Funktionen | entscheiden, aussuchen, planen, Struktur | „Man kann immer aussuchen, was man machen will, das fällt die aber noch schwer.“ (FD) |
| Kapitel 7 Bewegung | Bandscheibenvorfall, sitzen/stehen, Treppe | „2 Stunden am Stück sitzen ist schwierig, nur stehen ist auch schwierig“ (FS) → ohne Tabelle in die Ergänzenden Hinweise |
| Kapitel 5 Stoffwechsel | Gewicht, Übergewicht | „Sie ist jetzt konsequent dabei Gewicht zu verlieren.“ (FS) → Ergänzende Hinweise |
| Ergänzende Hinweise | OP, Reha, MRT, Ärzte, Arztwechsel, Schmerz | „Jetzt haben wir einen OP-Vorbesprechungstermin in Biberach.“ (FS) |

Drei Fallen: Behandlungen (OP, Reha) sind keine Körperfunktion. Stärken sind keine
Erläuterung. Eine im Gespräch genannte Krankheit ist keine Diagnose und begründet kein Kreuz.
