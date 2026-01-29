#include "dpt_model.h"
#include "dendrophone/graph/dpt_parser.h"
#include "dendrophone/graph/graph_ast.h"
#include <QPointF>
#include <QDebug>
#include <QMap>
#include <QSet>
#include <cmath>

DPTModel::DPTModel(QObject* parent) : QObject(parent) {}

QQmlListProperty<NodeModel> DPTModel::nodes() {
    return QQmlListProperty<NodeModel>(
        this,
        this,
        [](QQmlListProperty<NodeModel>* prop) -> qsizetype {
            auto* model = qobject_cast<DPTModel*>(prop->object);
            return model ? model->m_nodes.size() : 0;
        },
        [](QQmlListProperty<NodeModel>* prop, qsizetype index) -> NodeModel* {
            auto* model = qobject_cast<DPTModel*>(prop->object);
            return (model && index >= 0 && index < model->m_nodes.size()) ? model->m_nodes[index] : nullptr;
        }
    );
}

QVariantList DPTModel::edges() const {
    QVariantList result;
    for (const auto& edge : m_edges) {
        QVariantMap edgeMap;
        edgeMap["fromNode"] = edge.fromNode;
        edgeMap["fromPort"] = edge.fromPort;
        edgeMap["toNode"] = edge.toNode;
        edgeMap["toPort"] = edge.toPort;
        result.append(edgeMap);
    }
    return result;
}

bool DPTModel::loadDPT(const QString& filePath) {
    clearGraph();

    qDebug() << "Loading DPT from:" << filePath;

    // Call C++ DPT parser
    auto parseResult = DptParser::parse_file(filePath.toStdString());

    if (!parseResult.success) {
        m_errorMessage = QString::fromStdString(parseResult.errors.empty()
            ? "Unknown parse error"
            : parseResult.errors[0]);
        qDebug() << "Parse failed:" << m_errorMessage;
        emit errorMessageChanged();
        return false;
    }

    // Create NodeModel objects from Graph AST
    const auto& graph = parseResult.graph;

    qDebug() << "Loaded graph with" << graph.nodes.size() << "nodes and"
             << graph.connections.size() << "connections";

    for (const auto& [id, node] : graph.nodes) {
        createNodeFromAST(QString::fromStdString(id), node);
    }

    // Create edge data from connections
    for (const auto& conn : graph.connections) {
        EdgeData edge;
        edge.fromNode = QString::fromStdString(conn.from.node);
        edge.fromPort = QString::fromStdString(conn.from.port);
        edge.toNode = QString::fromStdString(conn.to.node);
        edge.toPort = QString::fromStdString(conn.to.port);
        m_edges.append(edge);
    }

    // Auto-layout nodes
    autoLayout();

    m_loaded = true;
    emit nodesChanged();
    emit edgesChanged();
    emit loadedChanged();

    qDebug() << "Graph loaded successfully";

    return true;
}

void DPTModel::autoLayout() {
    if (m_nodes.isEmpty()) return;

    // Separate LFO nodes from signal flow nodes
    QList<NodeModel*> lfoNodes;
    QList<NodeModel*> signalNodes;

    for (auto* node : m_nodes) {
        if (node->nodeType() == "lfo") {
            lfoNodes.append(node);
        } else {
            signalNodes.append(node);
        }
    }

    // Build adjacency map for layer calculation (signal nodes only)
    QMap<QString, QSet<QString>> outgoing;
    QMap<QString, int> incomingCount;

    for (auto* node : signalNodes) {
        outgoing[node->nodeId()] = QSet<QString>();
        incomingCount[node->nodeId()] = 0;
    }

    // Only count signal edges (not parameter connections from LFOs)
    for (const auto& edge : m_edges) {
        // Skip if edge is from an LFO (parameter modulation)
        bool fromLfo = false;
        for (auto* lfo : lfoNodes) {
            if (lfo->nodeId() == edge.fromNode) {
                fromLfo = true;
                break;
            }
        }
        if (fromLfo) continue;

        // Only process if both nodes are signal nodes
        if (outgoing.contains(edge.fromNode) && incomingCount.contains(edge.toNode)) {
            outgoing[edge.fromNode].insert(edge.toNode);
            incomingCount[edge.toNode]++;
        }
    }

    // Assign layers using BFS
    QMap<QString, int> nodeLayer;
    QList<QString> queue;

    for (auto* node : signalNodes) {
        if (incomingCount[node->nodeId()] == 0) {
            nodeLayer[node->nodeId()] = 0;
            queue.append(node->nodeId());
        }
    }

    while (!queue.isEmpty()) {
        QString current = queue.takeFirst();
        int currentLayer = nodeLayer[current];

        for (const QString& next : outgoing[current]) {
            int proposedLayer = currentLayer + 1;
            if (!nodeLayer.contains(next) || nodeLayer[next] < proposedLayer) {
                nodeLayer[next] = proposedLayer;
            }

            if (!queue.contains(next)) {
                queue.append(next);
            }
        }
    }

    for (auto* node : signalNodes) {
        if (!nodeLayer.contains(node->nodeId())) {
            nodeLayer[node->nodeId()] = 0;
        }
    }

    // Group signal nodes by layer
    QMap<int, QList<NodeModel*>> layers;
    for (auto* node : signalNodes) {
        int layer = nodeLayer[node->nodeId()];
        layers[layer].append(node);
    }

    // Position signal flow nodes
    const int layerSpacing = 350;
    const int nodeSpacing = 180;
    const int startX = 100;
    const int startY = 100;

    for (auto it = layers.begin(); it != layers.end(); ++it) {
        int layer = it.key();
        QList<NodeModel*>& nodesInLayer = it.value();

        for (int i = 0; i < nodesInLayer.size(); ++i) {
            int x = startX + layer * layerSpacing;
            int y = startY + i * nodeSpacing;
            nodesInLayer[i]->setPosition(QPointF(x, y));
        }
    }

    // Assign colors to LFO nodes
    QStringList lfoColors = {"#ff6b6b", "#4ecdc4", "#ffe66d", "#a8e6cf", "#ff8b94", "#c7ceea"};
    QMap<QString, QString> lfoColorMap;  // node ID -> color

    for (int i = 0; i < lfoNodes.size(); ++i) {
        QString color = lfoColors[i % lfoColors.size()];
        lfoNodes[i]->setLfoColor(color);
        lfoColorMap[lfoNodes[i]->nodeId()] = color;
    }

    // Track which LFOs connect to which nodes
    QMap<QString, QStringList> nodeLfoColors;  // target node -> list of LFO colors

    for (const auto& edge : m_edges) {
        if (lfoColorMap.contains(edge.fromNode)) {
            // This is an LFO -> parameter connection
            QString lfoColor = lfoColorMap[edge.fromNode];
            nodeLfoColors[edge.toNode].append(lfoColor);
        }
    }

    // Set connected LFO colors on nodes
    for (auto* node : signalNodes) {
        if (nodeLfoColors.contains(node->nodeId())) {
            node->setConnectedLfoColors(nodeLfoColors[node->nodeId()]);
        }
    }

    // Position LFO nodes at the bottom in a row
    const int lfoY = startY + 600;  // Below the main signal flow
    const int lfoSpacing = 250;

    for (int i = 0; i < lfoNodes.size(); ++i) {
        int x = startX + i * lfoSpacing;
        lfoNodes[i]->setPosition(QPointF(x, lfoY));
    }
}

void DPTModel::clearGraph() {
    qDeleteAll(m_nodes);
    m_nodes.clear();
    m_edges.clear();
    m_loaded = false;
}

void DPTModel::createNodeFromAST(const QString& id, const GraphNode& graphNode) {
    auto* node = new NodeModel(id, QString::fromStdString(graphNode.type), this);

    // Convert inputs
    QStringList inputs;
    for (const auto& input : graphNode.inputs) {
        inputs.append(QString::fromStdString(input));
    }
    node->setInputs(inputs);

    // Convert outputs
    QStringList outputs;
    for (const auto& output : graphNode.outputs) {
        outputs.append(QString::fromStdString(output));
    }
    node->setOutputs(outputs);

    // Convert param_inputs
    QStringList paramInputs;
    for (const auto& paramInput : graphNode.param_inputs) {
        paramInputs.append(QString::fromStdString(paramInput));
    }
    node->setParamInputs(paramInputs);

    // Convert params
    QVariantMap params;
    for (const auto& [key, value] : graphNode.params) {
        QString qkey = QString::fromStdString(key);
        if (auto* fval = std::get_if<float>(&value)) {
            params[qkey] = *fval;
        } else if (auto* ival = std::get_if<int>(&value)) {
            params[qkey] = *ival;
        } else if (auto* bval = std::get_if<bool>(&value)) {
            params[qkey] = *bval;
        } else if (auto* sval = std::get_if<std::string>(&value)) {
            params[qkey] = QString::fromStdString(*sval);
        }
    }
    node->setParams(params);

    m_nodes.append(node);
}

NodeModel* DPTModel::getNode(int index) {
    if (index >= 0 && index < m_nodes.size()) {
        return m_nodes[index];
    }
    return nullptr;
}
