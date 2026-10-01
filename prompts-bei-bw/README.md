# BEI_BW – KI-Prompts für die Übertragung von Gesprächsprotokollen in die Bögen

Ziel: Eine Mitarbeitende des Sozialen Dienstes (Landratsamt) überträgt mit Hilfe der
hauseigenen KI „Nele“ (ChatGPT-Modelle) die Rohnotizen eines Bedarfsermittlungs-
gesprächs in die BEI_BW-Vorlagen. Die KI ordnet Textstellen zu und übernimmt sie
**wortwörtlich**. Sie formuliert nicht um und erfindet nichts.

Quellen je Fall (werden in den Prompt eingefügt):

| Quelle | Inhalt | Beispiel 1 |
|---|---|---|
| Quelle 1 | Gesprächsprotokoll (Rohnotizen, Sprecherkürzel FS/FF/FD/SD, Ich-Form = Person) | `Beispiel_1_Gesprächsprotokoll_1.docx` |
| Quelle 2 | Erhebungsbogen C (bereits ausgefüllt, mit Lebensbereichen 1–9) | `Beispiel_1_BEI_BW_C_Bedarfsermittlung_1.docx` |

Zielbögen (je ein eigener Prompt):

| Nr. | Bogen | Prompt | Status |
|---|---|---|---|
| 1 | BEI_BW A – Basisbogen | [02_prompt_basisbogen.txt](02_prompt_basisbogen.txt) · [Seite für die Mitarbeitende](https://claude.ai/artifact/TpZRBTJ2EdCQZ6CVqKPU9L) ([HTML](04_seite_basisbogen_assistent.html)) | fertig, getestet an Beispiel 1 |
| 2 | BEI_BW C – Erhebungsbogen (Bedarfsermittlung) | [05_prompt_erhebungsbogen_c.txt](05_prompt_erhebungsbogen_c.txt) · [Seite](https://claude.ai/artifact/TKWT9qCYg8JVpRSMVtfWSR) ([HTML](07_seite_erhebungsbogen_assistent.html)) | fertig, getestet an Beispiel 1 |
| 2b | BEI_BW C, **direkt in Word** (ohne Kopieren, ohne Prompt-Anpassung) | [08_prompt_erhebungsbogen_c_direkt.txt](08_prompt_erhebungsbogen_c_direkt.txt) + [tools/fill_bei_bw_c.py](tools/fill_bei_bw_c.py) | fertig, getestet an Beispiel 1 |
| 3 | BEI_BW B – Gesundheitsbogen, direkt in Word | [09_prompt_gesundheitsbogen_b_direkt.txt](09_prompt_gesundheitsbogen_b_direkt.txt) + [tools/fill_bei_bw_b.py](tools/fill_bei_bw_b.py) · [Seite](https://claude.ai/artifact/NpwVRQrdNmGwS2N2tWSrKe) ([HTML](10_seite_gesundheitsbogen_assistent.html)) | fertig, getestet an Beispiel 1 |
| 4 | GP-Vorbereitung | – | danach (offene Frage, siehe unten) |

## Warum die bisherigen Versuche scheiterten

Zwei Fehlerbilder wurden genannt: falsche Formatierung und falsche Zuordnung.
Die Ursachen liegen in der Aufgabenstellung an die KI, nicht im Modell:

1. **Die KI sollte den Bogen „ausfüllen“.** Dann baut sie die Word-Vorlage als
   Markdown nach: Tabellen, fette Überschriften, `---` wird zur Trennlinie,
   Kästchen werden zu Aufzählungen. Die Word-Kästchen sind aber
   Inhaltssteuerelemente (46 Stück im Basisbogen). Text lässt sich dort nicht
   „hineinpasten“, man muss sie anklicken. Ein nachgebauter Bogen ist deshalb
   nie übertragbar.
2. **Zuordnung nach Themenähnlichkeit statt nach Felddefinition.** Beispiele aus
   Beispiel 1: „meine 3 Kinder“ (sagt die Pflegemutter) landet bei „Anzahl der
   Kinder“; „Bandscheibenvorfall“ führt zu einem Kreuz bei „körperliche
   Beeinträchtigung“ unter *wesentliche Behinderung*; die Werkstatt-Betreuerin
   wird zur rechtlichen Betreuerin.
3. **Lücken werden plausibel gefüllt** (Familienstand „ledig“, Staatsangehörigkeit
   „deutsch“), obwohl das Protokoll dazu nichts sagt.
4. **Sprecher gehen verloren.** Ohne Kürzel-Legende weiß die KI nicht, wer FS,
   FF, FD und SD sind, und schreibt Aussagen der Begleitpersonen der Person zu.

## Lösungsweg (gilt für alle Bögen)

**Ein Bogen = ein Prompt, und jeder Prompt liefert eine Ausfüllliste, keinen Bogen.**

Die Ausfüllliste hat für jedes Feld der Vorlage genau eine Zeile in der
Reihenfolge der Vorlage: Feldname, Eintrag, Status, Beleg. Die Mitarbeitende
geht die Liste von oben nach unten durch und überträgt: Text kopieren,
Kästchen anklicken. Layout und Inhalt sind damit getrennt, das Formatproblem
entfällt.

Jeder Prompt ist gleich aufgebaut:

1. **Rolle und Auftrag** (drei Sätze).
2. **Quellen und Kürzel-Legende.** Die Legende füllt die Mitarbeitende pro Fall
   aus. Ohne Legende darf die KI keine Funktion erfinden.
3. **Grundregeln:** nur Quellen, wortwörtlich mit Kürzel, drei Status
   (BELEGT / ABGELEITET / OFFEN), Kästchen nur mit Beleg, Aussagen anderer
   Sprecher über sich selbst zählen nicht, Gesundheitsangaben gehören in Bogen B.
4. **Feldkatalog mit Signalwörtern und Fallen.** Das ist die Schlüsselwort-Analyse
   aus [01_schluesselwoerter_zuordnung.md](01_schluesselwoerter_zuordnung.md),
   direkt in den Prompt eingebaut. Die KI sucht gezielt nach diesen Signalen
   statt nach Ähnlichkeit.
5. **Festes Ausgabeformat** in einem einzigen Codeblock. Dadurch rendert der
   Chat kein Markdown, `---` bleibt `---`, und die Liste lässt sich am Stück
   kopieren.
6. **Block „Nicht zugeordnet“** für Protokollstellen mit Bezug zum Bogen, die
   die KI keinem Feld sicher zuordnen kann. Das ersetzt das Raten.
7. **Selbstprüfung** in sechs Ja/Nein-Zeilen am Ende. Sie zwingt das Modell,
   die häufigsten Fehler noch einmal gegen die eigene Ausgabe zu prüfen.

## Ablauf für die Mitarbeitende

1. Prompt-Datei öffnen, gesamten Text kopieren.
2. Die Kürzel-Legende oben im Prompt für den Fall ausfüllen (wer ist FS, FF, FD, SD).
3. Gesprächsprotokoll als Text zwischen `=== QUELLE 1 ===` und `=== ENDE QUELLE 1 ===` einfügen.
4. Erhebungsbogen C als Text zwischen `=== QUELLE 2 ===` und `=== ENDE QUELLE 2 ===` einfügen.
5. Abschicken. Die Antwort ist ein Codeblock mit der Ausfüllliste.
6. Liste von oben nach unten in die Word-Vorlage übertragen.
7. Alle Zeilen mit Status **ABGELEITET** fachlich prüfen, alle **OFFEN** aus der Akte ergänzen.

## Besonderheit Bogen C: Hinweise der Vorlage

Die Vorlage C hat keine Kästchen, nur Text und zwei Tabellen. Deshalb liefert der
Prompt hier den kompletten Bogentext in Vorlagenreihenfolge, blockweise kopierbar.
Die Hinweistexte der Vorlage werden so behandelt:

| Hinweis in der Vorlage | Behandlung |
|---|---|
| „Siehe Vorbereitungsbogen“ (10 Tabellenzellen) | bleibt als erste Zeile stehen; passt eine Aussage, folgt „Ergänzung aus dem Gespräch am [Datum]:“ mit den Zitaten |
| „Aus dem Gespräch am XX.XX.XXXX:“ (9 Lebensbereiche) | Datum einsetzen, darunter die Zitate; ohne Zitat „Keine Einschränkungen ersichtlich.“ |
| „Aus dem Teilhabebericht vom XX.XX.XXXX:“ (Umweltfaktoren) | Datum einsetzen, Stichworte aus dem Teilhabebericht; liegt keiner vor, „Aus dem Gespräch am [Datum]:“ mit Stichworten und Kürzel |
| Kursive Erklärtexte unter den Überschriften | nicht ausgeben, stehen schon in der Vorlage |
| „Ergänzende Hinweise“ | Entwurf von drei Sätzen, gekennzeichnet „Entwurf, bitte prüfen“ |

Zusätzlich hat der Prompt eine Abdeckungsprüfung: Er zählt die Protokollzeilen und
listet nicht zugeordnete und mehrfach zugeordnete Sätze auf. Damit geht kein Satz
verloren und keiner steht doppelt im selben Block.

## Variante „direkt in Word“ (Bogen C)

Wunsch: nichts kopieren, nichts am Prompt anpassen. Dafür ist die Arbeit geteilt:

1. **Die KI ordnet zu.** Sie ermittelt Name, Gesprächsdatum und Sprecherkürzel selbst
   aus Vorlage und Protokoll und erzeugt eine Zuordnung als JSON (festes Schema,
   wörtliche Zitate je Block).
2. **Ein Skript schreibt.** `tools/fill_bei_bw_c.py` öffnet die Word-Vorlage, setzt
   die Datumsangaben in die Hinweiszeilen, hängt die Zitate als Absätze an und übernimmt
   dabei die Formatierung des jeweiligen Vorlagenabsatzes. Layout, Kopf- und Fußzeile,
   Tabellen und Aufzählungen bleiben unverändert. Zusätzlich entsteht ein
   Prüfprotokoll (`*_Pruefprotokoll.txt`) mit nicht und mehrfach zugeordneten Sätzen.

Das Skript steht vollständig im Prompt `08_prompt_erhebungsbogen_c_direkt.txt`. Die
Mitarbeitende lädt in Nele die Vorlage und das Protokoll hoch, fügt den Prompt ein und
erhält die ausgefüllte .docx zurück. Voraussetzung ist, dass Nele Python ausführen und
Dateien zurückgeben kann (bei ChatGPT die Funktion „Datenanalyse“ bzw. Code Interpreter).
Kann Nele das nicht, gibt sie nur das JSON aus; dann läuft das Skript einmal lokal:

```
pip install python-docx
python tools/fill_bei_bw_c.py Vorlage.docx zuordnung.json Ausgabe.docx
```

Unter Windows genügt `tools/fuellen.cmd Vorlage.docx zuordnung.json Ausgabe.docx`.

Getestet: `tools/zuordnung_beispiel1.json` ist die Zuordnung für Beispiel 1. Das Skript
erzeugt daraus die ausgefüllte Datei; Inhalt und Absatzstruktur entsprechen dem von
der Fachkraft ausgefüllten Bogen (Ergänzungszeilen in den Tabellen, „Aus dem Gespräch
am …“ je Lebensbereich, Aufzählung bei den Umweltfaktoren).

## Bogen B: Gesundheitsbogen (direkt in Word)

Der Gesundheitsbogen hat zwei Arten von Inhalten mit verschiedenen Quellen. Das ist
die zentrale Regel des Prompts:

| Inhalt | Quelle | Aus dem Gespräch? |
|---|---|---|
| Diagnosen (ICD-10) | Arztbrief, Gutachten, Teilhabebericht, früherer Gesundheitsbogen | nie |
| Kreuze bei den ICF-Codes (b110 …) und Kapitelübersicht | dieselben medizinischen Quellen; aus Diagnosetext abgeleitete Kreuze werden als „abgeleitet“ markiert | nie |
| Erläuterungen zu einzelnen Codes | wörtliche Zitate mit Kürzel, höchstens drei je Code | ja |
| Ergänzende Hinweise | alle gesundheitsbezogenen Zitate plus Entwurf „bitte prüfen“ | ja |

Liegt keine medizinische Quelle vor, bleibt der Bogen ohne Kreuze. Die Erläuterungen
stehen trotzdem beim passenden Code, und das Prüfprotokoll listet diese Codes als
„fachliche Bewertung nötig“. So sieht die Fachkraft sofort, wo sie aus der Akte
ankreuzen muss. Das Skript `tools/fill_bei_bw_b.py` setzt Kästchen (Inhaltssteuerelemente),
Diagnosenliste, Erläuterungsspalte und Hinweise; Codes, für die die Vorlage keine
Tabelle enthält, meldet es statt sie zu verlieren.

Hinweis: Eine leere Vorlage B lag nicht vor. `tools/Vorlage_BEI_BW_B_abgeleitet_aus_Beispiel1.docx`
ist aus dem ausgefüllten Beispiel 1 abgeleitet (nur Kapitel 1 als Tabelle) und dient
dem Test. Für den Einsatz die Originalvorlage des Ministeriums verwenden und den
Prompt einmal damit testen.

## Testlauf

[03_testlauf_beispiel1_basisbogen.md](03_testlauf_beispiel1_basisbogen.md) enthält
die Ausfüllliste, die der Prompt für Beispiel 1 liefern muss, und den Abgleich mit
dem tatsächlich ausgefüllten Basisbogen. Ergebnis: keine Fehlzuordnung, alle im
Protokoll enthaltenen Angaben gefunden, alle Fallen (Kinder, körperliche
Behinderung, Betreuerin) vermieden.

[06_testlauf_beispiel1_erhebungsbogen.md](06_testlauf_beispiel1_erhebungsbogen.md)
macht dasselbe für Bogen C: Sieben von neun Lebensbereichen sind identisch mit der
Referenz, die übrigen zwei weichen nur durch eine zusätzliche Zuordnung und durch
Sätze ab, die im gelieferten Protokoll fehlen. Keine Fehlzuordnung.

Bogen B wurde mit zwei Zuordnungen getestet (`tools/zuordnung_beispiel1_b.json`, nur
Gespräch, und eine Referenz mit Diagnose F70.0 und sechs Kreuzen wie im Beispiel):
Kreuze, Kapitelübersicht, Diagnose, Erläuterungen und Hinweise landen an der richtigen
Stelle; ein Code ohne Tabelle in der Vorlage wird im Prüfprotokoll gemeldet.

## Offene Punkte für die nächsten Prompts

- **Gesundheitsbogen B:** Diagnosen und ICF-Codes kommen nicht aus dem Gespräch,
  sondern aus Akte und Teilhabebericht. Aus dem Protokoll kommen nur die
  Spalte „Erläuterungen“ und die „Ergänzenden Hinweise“. Der Prompt braucht die
  Liste der b-Codes des Kapitels als Feldkatalog.
- **GP-Vorbereitung:** Das Beispiel ist in der dritten Person zusammengefasst
  („Frau Apfelkuchen leidet an …“), nicht wörtlich. Das widerspricht der Regel
  „wortwörtlich übernehmen“. Vor dem Prompt ist zu klären, ob die GP-Vorbereitung
  wörtliche Zitate pro Lebensbereich oder eine Zusammenfassung enthalten soll.
  Außerdem nennt die Vorlage als Grundlage „letzter GP, akt. Teilhabebericht,
  ggf. Vorbereitungsbogen“, also Dokumente von vor dem Gespräch.
