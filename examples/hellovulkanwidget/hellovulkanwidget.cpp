/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the QtVulkan module
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QCalendarWidget>
#include <QWindow>
#include "../hellovulkanwindow/worker.h"

int main(int argc, char **argv)
{
    // Let's report what is going on under the hood.
    qputenv("QVULKAN_DEBUG", "render");

    QApplication app(argc, argv);

    qDebug("Opening window. Main/gui thread is %p", QThread::currentThread());

    QWindow *window = new QWindow;
    window->setSurfaceType(QSurface::OpenGLSurface);

    QVulkanRenderLoop rl(window);
    rl.setFlags(QVulkanRenderLoop::UpdateContinuously
                | QVulkanRenderLoop::EnableValidation
                | QVulkanRenderLoop::DontReleaseOnObscure // because with createWindowContainer our embedded window apparently does not
                                                          // get the unexpose anyway so let's pretend this is what we wanted in the first place ;)
                // | QVulkanRenderLoop::Unthrottled
                );
    rl.setFramesInFlight(FRAMES_IN_FLIGHT);

    Worker worker(&rl);
    rl.setWorker(&worker);

    QWidget w;
    QVBoxLayout *vl = new QVBoxLayout;
    vl->addWidget(new QPushButton("Widgets + a native child window with Vulkan"));
    vl->addWidget(new QCalendarWidget);
    QWidget *ww = QWidget::createWindowContainer(window);
    ww->setMinimumHeight(300);
    vl->addWidget(ww);
    w.resize(400, 400);
    w.setLayout(vl);
    w.show();

    return app.exec();
}
