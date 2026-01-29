import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 720
    title: "Dendrophone DPT Visualizer"

    color: "#1e1e1e"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top toolbar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#2d2d2d"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 12

                Label {
                    text: "Dendrophone"
                    color: "#ffffff"
                    font.pixelSize: 16
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: dptModel.loaded ? "✓ Graph loaded" : "No graph"
                    color: dptModel.loaded ? "#4caf50" : "#999999"
                }
            }
        }

        // Main canvas area
        GraphCanvas {
            id: canvas
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        // Status bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: "#252525"

            Label {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: dptModel.nodeCount + " nodes, " + dptModel.edgeCount + " edges"
                color: "#cccccc"
                font.pixelSize: 12
            }
        }
    }
}
