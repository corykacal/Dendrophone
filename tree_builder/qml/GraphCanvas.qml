import QtQuick
import QtQuick.Controls

Item {
    id: root

    // Camera state
    property real cameraX: 100
    property real cameraY: 100
    property real cameraZoom: 1.0

    Rectangle {
        anchors.fill: parent
        color: "#1a1a1a"

        // Grid background
        Canvas {
            id: gridCanvas
            anchors.fill: parent

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()

                // Draw grid
                ctx.strokeStyle = "#2a2a2a"
                ctx.lineWidth = 1

                var gridSize = 50 * root.cameraZoom
                var offsetX = root.cameraX % gridSize
                var offsetY = root.cameraY % gridSize

                // Vertical lines
                for (var x = offsetX; x < width; x += gridSize) {
                    ctx.beginPath()
                    ctx.moveTo(x, 0)
                    ctx.lineTo(x, height)
                    ctx.stroke()
                }

                // Horizontal lines
                for (var y = offsetY; y < height; y += gridSize) {
                    ctx.beginPath()
                    ctx.moveTo(0, y)
                    ctx.lineTo(width, y)
                    ctx.stroke()
                }
            }
        }

        // Graph container
        Item {
            id: graphContainer
            x: root.cameraX
            y: root.cameraY
            scale: root.cameraZoom

            // Render edges first (below nodes)
            Repeater {
                model: dptModel.edges
                delegate: Component {
                    EdgeItem {
                        required property var modelData
                        fromNode: modelData.fromNode
                        fromPort: modelData.fromPort
                        toNode: modelData.toNode
                        toPort: modelData.toPort
                    }
                }
            }

            // Render nodes
            Repeater {
                id: nodeRepeater
                model: dptModel.nodeCount
                delegate: Component {
                    NodeItem {
                        required property int index
                        nodeModel: dptModel.getNode(index)
                    }
                }
                onCountChanged: {
                    console.log("Node repeater count:", count)
                }
            }
        }

        // Pan/zoom controls
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton

            property real lastX: 0
            property real lastY: 0

            onPressed: (mouse) => {
                lastX = mouse.x
                lastY = mouse.y
            }

            onPositionChanged: (mouse) => {
                if (pressed) {
                    // Pan camera
                    root.cameraX += (mouse.x - lastX)
                    root.cameraY += (mouse.y - lastY)
                    lastX = mouse.x
                    lastY = mouse.y
                    gridCanvas.requestPaint()
                }
            }

            onWheel: (wheel) => {
                // Zoom with scroll wheel
                var zoomDelta = wheel.angleDelta.y > 0 ? 0.1 : -0.1
                root.cameraZoom = Math.max(0.1, Math.min(3.0, root.cameraZoom + zoomDelta))
                gridCanvas.requestPaint()
            }
        }
    }
}
