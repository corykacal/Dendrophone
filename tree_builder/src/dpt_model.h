#ifndef DPT_MODEL_H
#define DPT_MODEL_H

#include <QObject>
#include <QQmlListProperty>
#include <QList>
#include <QVariantList>
#include "node_model.h"
#include "dendrophone/graph/graph_ast.h"

struct EdgeData {
    QString fromNode;
    QString fromPort;
    QString toNode;
    QString toPort;
};

class DPTModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<NodeModel> nodes READ nodes NOTIFY nodesChanged)
    Q_PROPERTY(QVariantList edges READ edges NOTIFY edgesChanged)
    Q_PROPERTY(int nodeCount READ nodeCount NOTIFY nodesChanged)
    Q_PROPERTY(int edgeCount READ edgeCount NOTIFY edgesChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit DPTModel(QObject* parent = nullptr);

    QQmlListProperty<NodeModel> nodes();
    QVariantList edges() const;
    int nodeCount() const { return m_nodes.size(); }
    int edgeCount() const { return m_edges.size(); }
    bool loaded() const { return m_loaded; }
    QString errorMessage() const { return m_errorMessage; }

public slots:
    bool loadDPT(const QString& filePath);
    void autoLayout();
    Q_INVOKABLE NodeModel* getNode(int index);

signals:
    void nodesChanged();
    void edgesChanged();
    void loadedChanged();
    void errorMessageChanged();

private:
    void clearGraph();
    void createNodeFromAST(const QString& id, const GraphNode& node);

    QList<NodeModel*> m_nodes;
    QList<EdgeData> m_edges;
    bool m_loaded = false;
    QString m_errorMessage;
};

#endif // DPT_MODEL_H
