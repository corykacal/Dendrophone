import QtQuick

Canvas {
    id: root

    required property string fromNode
    required property string fromPort
    required property string toNode
    required property string toPort

    width: 2000  // Large canvas to cover graph area
    height: 2000
    x: -1000  // Center canvas
    y: -1000

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()

        // Find source and destination port positions
        var fromPos = findPortPosition(fromNode, fromPort, true)
        var toPos = findPortPosition(toNode, toPort, false)

        if (!fromPos || !toPos) return

        // Draw bezier curve
        ctx.strokeStyle = "#4a9eff"
        ctx.lineWidth = 2

        var controlPointOffset = Math.min(150, Math.abs(toPos.x - fromPos.x) / 2)

        ctx.beginPath()
        ctx.moveTo(fromPos.x + 1000, fromPos.y + 1000)
        ctx.bezierCurveTo(
            fromPos.x + controlPointOffset + 1000, fromPos.y + 1000,
            toPos.x - controlPointOffset + 1000, toPos.y + 1000,
            toPos.x + 1000, toPos.y + 1000
        )
        ctx.stroke()
    }

    function findPortPosition(nodeId, portName, isOutput) {
        // Find the node
        for (var i = 0; i < dptModel.nodeCount; i++) {
            var node = dptModel.getNode(i);
            if (!node || node.nodeId !== nodeId) continue;

            // Find port index in the appropriate port list
            var ports = isOutput ? node.outputs : node.inputs
            var portIndex = -1
            for (var j = 0; j < ports.length; j++) {
                if (ports[j] === portName) {
                    portIndex = j
                    break
                }
            }

            if (portIndex < 0) {
                // Check param_inputs for control connections
                if (!isOutput) {
                    ports = node.paramInputs
                    for (var k = 0; k < ports.length; k++) {
                        if (ports[k] === portName) {
                            portIndex = node.inputs.length + k
                            break
                        }
                    }
                }
                if (portIndex < 0) return null
            }

            // Calculate port position to match NodeItem layout
            var nodeWidth = 200
            var nodeBoxMargin = 10
            var portSpacing = 20
            var portYOffset = 8  // Half of port item height (16px)

            // Estimate node box height (should match NodeItem calculation)
            var numParams = Object.keys(node.params).length
            var contentHeight = 60 + (numParams > 0 ? numParams * 11 + 12 : 0)
            var nodeBoxHeight = Math.max(80, contentHeight + 24)

            // Port positions are centered vertically on the nodeBox
            var portsCount = isOutput ? node.outputs.length : (node.inputs.length + node.paramInputs.length)
            var firstPortY = nodeBoxMargin + nodeBoxHeight / 2 - (portsCount - 1) * portSpacing / 2
            var portY = firstPortY + portIndex * portSpacing + portYOffset

            var portX = isOutput ? (nodeWidth) : 0

            var x = node.posX + portX
            var y = node.posY + portY

            return { x: x, y: y }
        }
        return null
    }

    // Repaint when nodes change
    Connections {
        target: dptModel
        function onNodesChanged() {
            root.requestPaint()
        }
    }

    Component.onCompleted: {
        requestPaint()
    }
}
