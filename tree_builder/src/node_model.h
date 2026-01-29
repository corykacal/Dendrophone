#ifndef NODE_MODEL_H
#define NODE_MODEL_H

#include <QObject>
#include <QString>
#include <QPointF>
#include <QStringList>
#include <QVariantMap>

class NodeModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString nodeId READ nodeId CONSTANT)
    Q_PROPERTY(QString nodeType READ nodeType CONSTANT)
    Q_PROPERTY(QPointF position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(qreal posX READ posX NOTIFY positionChanged)
    Q_PROPERTY(qreal posY READ posY NOTIFY positionChanged)
    Q_PROPERTY(QStringList inputs READ inputs CONSTANT)
    Q_PROPERTY(QStringList outputs READ outputs CONSTANT)
    Q_PROPERTY(QStringList paramInputs READ paramInputs CONSTANT)
    Q_PROPERTY(QVariantMap params READ params CONSTANT)
    Q_PROPERTY(QString lfoColor READ lfoColor WRITE setLfoColor NOTIFY lfoColorChanged)
    Q_PROPERTY(QStringList connectedLfoColors READ connectedLfoColors WRITE setConnectedLfoColors NOTIFY connectedLfoColorsChanged)

public:
    explicit NodeModel(const QString& id, const QString& type, QObject* parent = nullptr);

    QString nodeId() const { return m_id; }
    QString nodeType() const { return m_type; }
    QPointF position() const { return m_position; }
    qreal posX() const { return m_position.x(); }
    qreal posY() const { return m_position.y(); }
    QStringList inputs() const { return m_inputs; }
    QStringList outputs() const { return m_outputs; }
    QStringList paramInputs() const { return m_paramInputs; }
    QVariantMap params() const { return m_params; }
    QString lfoColor() const { return m_lfoColor; }
    QStringList connectedLfoColors() const { return m_connectedLfoColors; }

    void setPosition(const QPointF& pos);
    void setInputs(const QStringList& inputs) { m_inputs = inputs; }
    void setOutputs(const QStringList& outputs) { m_outputs = outputs; }
    void setParamInputs(const QStringList& paramInputs) { m_paramInputs = paramInputs; }
    void setParams(const QVariantMap& params) { m_params = params; }
    void setLfoColor(const QString& color);
    void setConnectedLfoColors(const QStringList& colors);

signals:
    void positionChanged();
    void lfoColorChanged();
    void connectedLfoColorsChanged();

private:
    QString m_id;
    QString m_type;
    QPointF m_position;
    QStringList m_inputs;
    QStringList m_outputs;
    QStringList m_paramInputs;
    QVariantMap m_params;
    QString m_lfoColor;
    QStringList m_connectedLfoColors;
};

#endif // NODE_MODEL_H
