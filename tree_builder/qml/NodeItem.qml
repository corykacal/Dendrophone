import QtQuick
import QtQuick.Controls

Item {
    id: root

    required property var nodeModel

    x: nodeModel ? nodeModel.posX : 0
    y: nodeModel ? nodeModel.posY : 0
    width: 200
    height: nodeContent.height + 40

    Component.onCompleted: {
        if (nodeModel) {
            console.log("NodeItem created:", nodeModel.nodeId,
                        "at position (", nodeModel.posX, ",", nodeModel.posY, ")")
        } else {
            console.log("NodeItem created with null model")
        }
    }

    // Main node box
    Rectangle {
        id: nodeBox
        anchors.fill: parent
        anchors.margins: 10  // Space for port circles
        color: nodeModel && nodeModel.lfoColor ? nodeModel.lfoColor : "#2d2d2d"
        border.color: nodeModel && nodeModel.lfoColor ? "#ffffff" : "#4a4a4a"
        border.width: 2
        radius: 8

        // Node content
        Column {
            id: nodeContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            spacing: 6

            // Node ID and type
            Label {
                text: nodeModel ? nodeModel.nodeId : ""
                color: "#ffffff"
                font.pixelSize: 14
                font.bold: true
                width: parent.width
                elide: Text.ElideMiddle
            }

            Label {
                text: nodeModel ? nodeModel.nodeType : ""
                color: "#999999"
                font.pixelSize: 10
                width: parent.width
            }

            // Parameters
            Column {
                width: parent.width
                spacing: 2
                visible: nodeModel && Object.keys(nodeModel.params).length > 0

                Repeater {
                    model: nodeModel ? Object.keys(nodeModel.params) : []
                    delegate: Label {
                        required property string modelData
                        text: modelData + ": " + nodeModel.params[modelData]
                        color: "#aaaaaa"
                        font.pixelSize: 9
                        width: parent.width
                        elide: Text.ElideRight
                    }
                }
            }
        }

        // Hover effect
        Rectangle {
            anchors.fill: parent
            color: "#ffffff"
            opacity: mouseArea.containsMouse ? 0.1 : 0
            radius: parent.radius
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
        }

        // LFO indicator circles on top-right
        Row {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 4
            spacing: 4
            visible: nodeModel && nodeModel.connectedLfoColors.length > 0

            Repeater {
                model: nodeModel ? nodeModel.connectedLfoColors : []
                delegate: Rectangle {
                    required property string modelData
                    width: 16
                    height: 16
                    radius: 8
                    color: modelData
                    border.color: "#ffffff"
                    border.width: 2
                }
            }
        }
    }

    // Input ports (left side)
    Column {
        id: inputPorts
        anchors.left: parent.left
        anchors.verticalCenter: nodeBox.verticalCenter
        spacing: 20

        Repeater {
            model: nodeModel ? nodeModel.inputs : []
            delegate: Item {
                required property string modelData
                required property int index
                width: 80
                height: 16

                Rectangle {
                    id: portCircle
                    width: 12
                    height: 12
                    radius: 6
                    color: "#4a9eff"
                    border.color: "#ffffff"
                    border.width: 1
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                }

                Label {
                    text: modelData
                    color: "#cccccc"
                    font.pixelSize: 9
                    anchors.left: portCircle.right
                    anchors.leftMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    // Output ports (right side)
    Column {
        id: outputPorts
        anchors.right: parent.right
        anchors.verticalCenter: nodeBox.verticalCenter
        spacing: 20

        Repeater {
            model: nodeModel ? nodeModel.outputs : []
            delegate: Item {
                required property string modelData
                required property int index
                width: 80
                height: 16

                Label {
                    text: modelData
                    color: "#cccccc"
                    font.pixelSize: 9
                    anchors.right: portCircle.left
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    id: portCircle
                    width: 12
                    height: 12
                    radius: 6
                    color: "#4a9eff"
                    border.color: "#ffffff"
                    border.width: 1
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    // Helper function to get port position
    function getInputPortPosition(portName) {
        var portIndex = nodeModel.inputs.indexOf(portName)
        if (portIndex < 0) return null
        var yOffset = nodeBox.height / 2 - (nodeModel.inputs.length - 1) * 10 + portIndex * 20
        return Qt.point(posX, posY + yOffset)
    }

    function getOutputPortPosition(portName) {
        var portIndex = nodeModel.outputs.indexOf(portName)
        if (portIndex < 0) return null
        var yOffset = nodeBox.height / 2 - (nodeModel.outputs.length - 1) * 10 + portIndex * 20
        return Qt.point(posX + width, posY + yOffset)
    }
}
