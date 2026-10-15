// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/path.h"

namespace cutefish::ui {

void Path::moveTo(PointF point)
{
    m_verbs.push_back(Verb::Move);
    m_points.push_back(point);
}

void Path::lineTo(PointF point)
{
    m_verbs.push_back(Verb::Line);
    m_points.push_back(point);
}

void Path::quadTo(PointF control, PointF end)
{
    m_verbs.push_back(Verb::Quad);
    m_points.push_back(control);
    m_points.push_back(end);
}

void Path::cubicTo(PointF control1, PointF control2, PointF end)
{
    m_verbs.push_back(Verb::Cubic);
    m_points.push_back(control1);
    m_points.push_back(control2);
    m_points.push_back(end);
}

void Path::close()
{
    m_verbs.push_back(Verb::Close);
}

void Path::clear()
{
    m_verbs.clear();
    m_points.clear();
}

} // namespace cutefish::ui
