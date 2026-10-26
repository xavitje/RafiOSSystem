// RafiHub – de interface
// Bediening: muis, toetsenbord (pijltjes, Enter, B, Ctrl+F) of controller (A/B/X/Y, d-pad).

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: venster
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 560
    visible: true
    title: "RafiHub"
    color: thema.achtergrond

    // ---------- Kleuren en maten ----------
    QtObject {
        id: thema
        readonly property color achtergrond: "#0c0f1d"
        readonly property color paneel: "#141a2e"
        readonly property color paneelLicht: "#1d2441"
        readonly property color rand: "#2a3256"
        readonly property color blauw: "#4ea1ff"
        readonly property color paars: "#b04cff"
        readonly property color tekst: "#eef1fb"
        readonly property color stil: "#8b93b5"
        readonly property color goed: "#3ddc97"
        readonly property color rood: "#ff6b81"
        readonly property int straal: 12
    }

    // ---------- Welke game is gekozen ----------
    property string gekozenId: ""
    property int wijzigingen: 0 // telt mee zodat "gekozen" opnieuw wordt berekend
    readonly property var gekozen: {
        wijzigingen; // afhankelijkheid
        const rij = raster.currentIndex
        return (rij >= 0 && rij < hub.games.aantal) ? hub.games.get(rij) : ({})
    }
    readonly property bool heeftGekozen: gekozen.gameId !== undefined

    function herstelKeuze() {
        const rij = hub.games.rijVan(gekozenId)
        raster.currentIndex = rij >= 0 ? rij : (hub.games.aantal > 0 ? 0 : -1)
        wijzigingen++
    }
    function speel(benchmark) {
        if (heeftGekozen)
            hub.speel(gekozen.gameId, benchmark)
    }

    Connections {
        target: hub.games
        function onDataChanged() { venster.wijzigingen++ }
        function onLayoutChanged() { venster.herstelKeuze() }
        function onModelReset() { venster.herstelKeuze() }
        function onRowsInserted() { venster.herstelKeuze() }
        function onRowsRemoved() { venster.herstelKeuze() }
    }

    // ---------- Sneltoetsen ----------
    Shortcut { sequences: ["Ctrl+F", "/"]; onActivated: zoekveld.forceActiveFocus() }
    Shortcut { sequence: "F5"; onActivated: hub.vernieuw() }
    Shortcut { sequence: "Ctrl+N"; onActivated: toevoegen.openLeeg() }

    // ---------- Controller ----------
    Connections {
        target: gamepad
        enabled: venster.active && !toevoegen.visible
        function onRichting(dx, dy) {
            raster.forceActiveFocus()
            if (dx > 0) raster.moveCurrentIndexRight()
            if (dx < 0) raster.moveCurrentIndexLeft()
            if (dy > 0) raster.moveCurrentIndexDown()
            if (dy < 0) raster.moveCurrentIndexUp()
        }
        function onBevestig() { venster.speel(false) }
        function onExtra() { venster.speel(true) }
        function onZoeken() { zoekveld.forceActiveFocus() }
        function onTerug() {
            if (zoekveld.activeFocus) {
                zoekveld.text = ""
                raster.forceActiveFocus()
            }
        }
    }

    // ---------- Kleine bouwstenen ----------
    component KnopStijl: Button {
        id: knop
        property bool hoofdknop: false
        property bool gevaar: false
        font.pixelSize: 14
        leftPadding: 16
        rightPadding: 16
        topPadding: 9
        bottomPadding: 9
        contentItem: Label {
            text: knop.text
            font: knop.font
            color: knop.gevaar ? thema.rood : thema.tekst
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 10
            color: knop.checked || knop.hovered ? thema.paneelLicht : "transparent"
            border.width: knop.hoofdknop ? 0 : 1
            border.color: knop.checked || knop.activeFocus ? thema.blauw : thema.rand
            opacity: knop.enabled ? (knop.down ? 0.8 : 1) : 0.4
            // Hoofdknop: blauw naar paars
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                visible: knop.hoofdknop
                opacity: knop.hovered ? 0.88 : 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: thema.blauw }
                    GradientStop { position: 1; color: thema.paars }
                }
            }
        }
    }

    component Hoesje: Item {
        id: hoesje
        property string bron: ""
        property string titel: ""
        property string letters: ""
        property color kleur: thema.paneelLicht
        property int ronding: thema.straal
        property color omgeving: thema.achtergrond // kleur eromheen, voor de ronde hoeken

        // Geen plaatje (of kapot): kleurvlak met beginletters
        Rectangle {
            anchors.fill: parent
            radius: hoesje.ronding
            visible: plaatje.status !== Image.Ready
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.lighter(hoesje.kleur, 1.25) }
                GradientStop { position: 1; color: Qt.darker(hoesje.kleur, 1.9) }
            }
            Label {
                anchors.centerIn: parent
                text: hoesje.letters
                color: "white"
                font.pixelSize: hoesje.width * 0.28
                font.bold: true
                opacity: 0.9
            }
            Label {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12 }
                text: hoesje.titel
                color: "white"
                font.pixelSize: Math.max(11, hoesje.width * 0.075)
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.85
            }
        }
        Image {
            id: plaatje
            anchors.fill: parent
            source: hoesje.bron
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            smooth: true
            visible: status === Image.Ready
        }
        // Ronde hoeken: een dikke rand in de achtergrondkleur bedekt de hoeken van het plaatje
        Rectangle {
            anchors.fill: parent
            anchors.margins: -8
            radius: hoesje.ronding + 8
            color: "transparent"
            border.width: 8
            border.color: hoesje.omgeving
        }
    }

    // ---------- Indeling ----------
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ===== Kopbalk =====
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 76
            color: thema.paneel

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 14

                Image {
                    source: "qrc:/rafihub.svg"
                    sourceSize: Qt.size(40, 40)
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                }
                ColumnLayout {
                    spacing: 0
                    Label { text: "RafiHub"; color: thema.tekst; font.pixelSize: 22; font.bold: true }
                    Label { text: "game-launcher van RafiOS"; color: thema.stil; font.pixelSize: 12 }
                }
                Item { Layout.fillWidth: true }

                TextField {
                    id: zoekveld
                    Layout.preferredWidth: 280
                    placeholderText: "Zoek een game…   (Ctrl+F)"
                    placeholderTextColor: thema.stil
                    color: thema.tekst
                    font.pixelSize: 14
                    leftPadding: 14
                    background: Rectangle {
                        radius: 10
                        color: thema.achtergrond
                        border.width: 1
                        border.color: zoekveld.activeFocus ? thema.blauw : thema.rand
                    }
                    onTextChanged: hub.games.zoekTekst = text
                    Keys.onEscapePressed: { text = ""; raster.forceActiveFocus() }
                    Keys.onDownPressed: raster.forceActiveFocus()
                    Keys.onReturnPressed: raster.forceActiveFocus()
                }

                Row {
                    spacing: 6
                    Repeater {
                        model: [["alles", "Alles"], ["steam", "Steam"], ["eigen", "Eigen games"]]
                        delegate: KnopStijl {
                            required property var modelData
                            text: modelData[1]
                            checkable: true
                            checked: hub.games.bron === modelData[0]
                            onClicked: hub.games.bron = modelData[0]
                        }
                    }
                }

                KnopStijl {
                    text: "+  Game toevoegen"
                    hoofdknop: true
                    onClicked: toevoegen.openLeeg()
                }
                KnopStijl {
                    text: "↻"
                    font.pixelSize: 18
                    ToolTip.visible: hovered
                    ToolTip.text: "Opnieuw zoeken naar games (F5)"
                    onClicked: hub.vernieuw()
                }
            }
        }

        // ===== Midden: raster + detailpaneel =====
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                GridView {
                    id: raster
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.topMargin: 10
                    clip: true
                    focus: true
                    cellWidth: 196
                    cellHeight: 330
                    model: hub.games
                    keyNavigationEnabled: true
                    highlightMoveDuration: 120
                    currentIndex: 0
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    onCurrentIndexChanged: {
                        const g = hub.games.get(currentIndex)
                        if (g.gameId !== undefined)
                            venster.gekozenId = g.gameId
                        venster.wijzigingen++
                    }
                    Keys.onReturnPressed: venster.speel(false)
                    Keys.onEnterPressed: venster.speel(false)
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_B) { venster.speel(true); event.accepted = true }
                    }

                    delegate: Item {
                        id: tegel
                        required property int index
                        required property string gameId
                        required property string titel
                        required property string bron
                        required property string cover
                        required property string letters
                        required property string kleur
                        readonly property bool gekozen: GridView.isCurrentItem
                        width: raster.cellWidth
                        height: raster.cellHeight

                        Item {
                            id: kaart
                            x: 10 // ruimte voor de gloed en het vergroten
                            y: 10
                            width: 176
                            height: 300
                            scale: tegel.gekozen ? 1.04 : (muis.containsMouse ? 1.02 : 1)
                            Behavior on scale { NumberAnimation { duration: 120 } }

                            Hoesje {
                                width: 176
                                height: 264
                                bron: tegel.cover
                                titel: tegel.titel
                                letters: tegel.letters
                                kleur: tegel.kleur
                            }
                            // Rand om de gekozen game (boven het hoesje, anders valt hij onder het hoekmasker)
                            Rectangle {
                                x: -5; y: -5
                                width: 186; height: 274
                                radius: thema.straal + 4
                                color: "transparent"
                                border.width: 3
                                border.color: thema.blauw
                                visible: tegel.gekozen
                            }
                            Rectangle { // bron-labeltje
                                x: 8; y: 8
                                radius: 6
                                color: "#cc0c0f1d"
                                width: bronTekst.implicitWidth + 12
                                height: bronTekst.implicitHeight + 6
                                Label {
                                    id: bronTekst
                                    anchors.centerIn: parent
                                    text: tegel.bron === "steam" ? "STEAM" : "EIGEN"
                                    font.pixelSize: 10
                                    font.bold: true
                                    font.letterSpacing: 1
                                    color: tegel.bron === "steam" ? thema.blauw : thema.paars
                                }
                            }
                            Label {
                                y: 272
                                width: 176
                                text: tegel.titel
                                color: tegel.gekozen ? thema.tekst : thema.stil
                                font.pixelSize: 14
                                font.bold: tegel.gekozen
                                elide: Text.ElideRight
                            }
                        }
                        MouseArea {
                            id: muis
                            anchors.fill: kaart
                            hoverEnabled: true
                            onClicked: { raster.currentIndex = tegel.index; raster.forceActiveFocus() }
                            onDoubleClicked: venster.speel(false)
                        }
                    }
                }

                // Niets te zien
                Column {
                    anchors.centerIn: parent
                    spacing: 12
                    visible: hub.games.aantal === 0
                    width: Math.min(parent.width - 48, 460)
                    Label {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: hub.totaal === 0 ? "Nog geen games gevonden" : "Geen games gevonden"
                        color: thema.tekst
                        font.pixelSize: 22
                        font.bold: true
                    }
                    Label {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: thema.stil
                        font.pixelSize: 14
                        text: hub.totaal === 0
                              ? "Installeer games in Steam, of voeg een .exe of programma toe."
                              : "Niets past bij je zoekopdracht of filter."
                    }
                    KnopStijl {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: hub.totaal === 0
                        hoofdknop: true
                        text: "+  Game toevoegen"
                        onClicked: toevoegen.openLeeg()
                    }
                }
            }

            // ===== Detailpaneel =====
            Rectangle {
                Layout.preferredWidth: 360
                Layout.fillHeight: true
                color: thema.paneel

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 14
                    visible: venster.heeftGekozen

                    Hoesje {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Math.min(220, parent.width)
                        Layout.preferredHeight: Layout.preferredWidth * 1.5
                        bron: venster.gekozen.cover || ""
                        titel: venster.gekozen.titel || ""
                        letters: venster.gekozen.letters || ""
                        kleur: venster.gekozen.kleur || thema.paneelLicht
                        omgeving: thema.paneel
                    }
                    Label {
                        Layout.fillWidth: true
                        text: venster.gekozen.titel || ""
                        color: thema.tekst
                        font.pixelSize: 22
                        font.bold: true
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        color: thema.stil
                        font.pixelSize: 13
                        text: (venster.gekozen.bron === "steam" ? "Steam" : "Eigen game")
                              + "  ·  " + hub.laatstTekst(venster.gekozen.laatstGespeeld || 0)
                    }

                    KnopStijl {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        hoofdknop: true
                        text: "▶   Spelen"
                        font.pixelSize: 17
                        font.bold: true
                        topPadding: 14
                        bottomPadding: 14
                        onClicked: venster.speel(false)
                    }
                    KnopStijl {
                        Layout.fillWidth: true
                        text: venster.gekozen.bron === "steam" ? "Benchmark-optie kopiëren" : "Spelen met benchmark"
                        onClicked: venster.speel(true)
                        ToolTip.visible: hovered
                        ToolTip.delay: 400
                        ToolTip.text: venster.gekozen.bron === "steam"
                                      ? "Kopieert de opstartoptie voor rafi-bench. Plak die in Steam bij Eigenschappen > Opstartopties."
                                      : "Start via rafi-bench: meet je fps en vergelijkt met de vorige keer."
                    }

                    // Fps-overlay
                    RowLayout {
                        Layout.fillWidth: true
                        visible: venster.gekozen.bron === "eigen"
                        Label {
                            Layout.fillWidth: true
                            text: "Fps-overlay (MangoHud)"
                            color: thema.tekst
                            font.pixelSize: 14
                        }
                        Switch {
                            id: mangohudSchakelaar
                            checked: venster.gekozen.mangohud === true
                            onToggled: hub.zetMangohud(venster.gekozen.gameId, checked)
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: venster.gekozen.bron === "steam"
                        wrapMode: Text.Wrap
                        color: thema.stil
                        font.pixelSize: 12
                        text: "Fps-overlay voor Steam-games: zet MANGOHUD=1 %command% bij de opstartopties in Steam, en wissel tijdens het spelen met rechter-Shift + F12."
                    }

                    Item { Layout.fillHeight: true }

                    KnopStijl {
                        Layout.fillWidth: true
                        visible: venster.gekozen.bron === "eigen"
                        gevaar: true
                        text: "Uit RafiHub halen"
                        onClicked: hub.verwijder(venster.gekozen.gameId)
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        color: thema.stil
                        font.pixelSize: 11
                        text: gamepad.verbonden ? "A spelen  ·  X benchmark  ·  Y zoeken"
                                                : "Enter spelen  ·  B benchmark  ·  Ctrl+F zoeken"
                    }
                }
            }
        }

        // ===== Statusbalk =====
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: thema.achtergrond
            Rectangle { width: parent.width; height: 1; color: thema.rand }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 8
                Rectangle {
                    width: 9; height: 9; radius: 5
                    color: hub.rafituneActief ? thema.goed : thema.stil
                }
                Label { text: hub.rafitune; color: thema.stil; font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                Label {
                    visible: gamepad.verbonden
                    text: "🎮  " + gamepad.naam
                    color: thema.stil
                    font.pixelSize: 12
                    Layout.rightMargin: 16
                }
                Label {
                    color: thema.stil
                    font.pixelSize: 12
                    text: hub.totaal + (hub.totaal === 1 ? " game" : " games") + "  ·  "
                          + (hub.steamMap !== "" ? "Steam gevonden" : "Steam niet gevonden")
                }
            }
        }
    }

    // ---------- Meldingen ----------
    Rectangle {
        id: melding
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 56
        width: Math.min(meldingTekst.implicitWidth + 40, venster.width - 80)
        height: meldingTekst.implicitHeight + 22
        radius: 12
        color: thema.paneelLicht
        border.color: thema.rand
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 180 } }
        Label {
            id: meldingTekst
            anchors.centerIn: parent
            width: Math.min(implicitWidth, venster.width - 120)
            wrapMode: Text.Wrap
            color: thema.tekst
            font.pixelSize: 14
        }
        Timer { id: meldingTimer; interval: 4500; onTriggered: melding.opacity = 0 }
        Connections {
            target: hub
            function onMelding(tekst) {
                meldingTekst.text = tekst
                melding.opacity = 1
                meldingTimer.restart()
            }
        }
    }

    // ---------- Game toevoegen ----------
    Dialog {
        id: toevoegen
        title: "Game toevoegen"
        modal: true
        anchors.centerIn: parent
        width: 520
        padding: 24

        function openLeeg() {
            bestandVeld.text = ""
            naamVeld.text = ""
            hoesjeVeld.text = ""
            foutTekst.text = ""
            open()
        }
        function toevoegenNu() {
            const fout = hub.voegToe(naamVeld.text, bestandVeld.text, hoesjeVeld.text)
            if (fout === "") {
                close()
                raster.forceActiveFocus()
            } else {
                foutTekst.text = fout
            }
        }

        background: Rectangle { radius: 16; color: thema.paneel; border.color: thema.rand }
        header: Label {
            text: toevoegen.title
            color: thema.tekst
            font.pixelSize: 20
            font.bold: true
            padding: 24
            bottomPadding: 0
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: thema.stil
                font.pixelSize: 13
                text: "Kies een .exe of .msi (start met Proton), of een Linux-programma of script."
            }
            Label { text: "Bestand"; color: thema.tekst; font.pixelSize: 13 }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: bestandVeld
                    Layout.fillWidth: true
                    readOnly: true
                    placeholderText: "Nog niets gekozen"
                    placeholderTextColor: thema.stil
                    color: thema.tekst
                    background: Rectangle { radius: 8; color: thema.achtergrond; border.color: thema.rand }
                }
                KnopStijl { text: "Kiezen…"; onClicked: bestandKiezer.open() }
            }
            Label { text: "Naam"; color: thema.tekst; font.pixelSize: 13 }
            TextField {
                id: naamVeld
                Layout.fillWidth: true
                placeholderText: "Naam van de game"
                placeholderTextColor: thema.stil
                color: thema.tekst
                background: Rectangle { radius: 8; color: thema.achtergrond; border.color: thema.rand }
                Keys.onReturnPressed: toevoegen.toevoegenNu()
            }
            Label { text: "Hoesje (mag leeg blijven)"; color: thema.tekst; font.pixelSize: 13 }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: hoesjeVeld
                    Layout.fillWidth: true
                    readOnly: true
                    placeholderText: "Geen plaatje: RafiHub maakt er een"
                    placeholderTextColor: thema.stil
                    color: thema.tekst
                    background: Rectangle { radius: 8; color: thema.achtergrond; border.color: thema.rand }
                }
                KnopStijl { text: "Kiezen…"; onClicked: hoesjeKiezer.open() }
            }
            Label {
                id: foutTekst
                Layout.fillWidth: true
                visible: text !== ""
                wrapMode: Text.Wrap
                color: thema.rood
                font.pixelSize: 13
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Item { Layout.fillWidth: true }
                KnopStijl { text: "Annuleren"; onClicked: toevoegen.close() }
                KnopStijl { text: "Toevoegen"; hoofdknop: true; onClicked: toevoegen.toevoegenNu() }
            }
        }
    }

    FileDialog {
        id: bestandKiezer
        title: "Kies een game"
        nameFilters: ["Games en programma's (*.exe *.EXE *.msi *.MSI *.sh *.x86_64 *.AppImage)",
                      "Alle bestanden (*)"]
        onAccepted: {
            bestandVeld.text = selectedFile
            if (naamVeld.text === "")
                naamVeld.text = hub.titelVoorPad(selectedFile)
        }
    }
    FileDialog {
        id: hoesjeKiezer
        title: "Kies een hoesje"
        nameFilters: ["Plaatjes (*.jpg *.jpeg *.png *.webp)"]
        onAccepted: hoesjeVeld.text = selectedFile
    }

    Component.onCompleted: {
        herstelKeuze()
        raster.forceActiveFocus()
        if (openToevoegen)
            toevoegen.openLeeg()
        if (zelftestModus) {
            // Ook het toevoegvenster één keer openen, zodat fouten daarin opvallen
            toevoegen.openLeeg()
            toevoegen.close()
        }
    }
}
