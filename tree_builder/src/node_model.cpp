#include "node_model.h"

NodeModel::NodeModel(const QString& id, const QString& type, QObject* parent)
    : QObject(parent), m_id(id), m_type(type), m_position(0, 0) {
}

void NodeModel::setPosition(const QPointF& pos) {
    if (m_position != pos) {
        m_position = pos;
        emit positionChanged();
    }
}

void NodeModel::setLfoColor(const QString& color) {
    if (m_lfoColor != color) {
        m_lfoColor = color;
        emit lfoColorChanged();
    }
}

void NodeModel::setConnectedLfoColors(const QStringList& colors) {
    if (m_connectedLfoColors != colors) {
        m_connectedLfoColors = colors;
        emit connectedLfoColorsChanged();
    }
}
