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

#ifndef WORKER_H
#define WORKER_H

#include <QVulkanRenderLoop>
#include <QThreadPool>

const int FRAMES_IN_FLIGHT = 2;

class Worker : public QVulkanFrameWorker
{
public:
    Worker(QVulkanRenderLoop *rl) : m_renderLoop(rl) { }

    void init() override;
    void cleanup() override;
    void queueFrame(int frame, VkQueue queue, VkSemaphore waitSem, VkSemaphore signalSem) override;

private:
    QVulkanRenderLoop *m_renderLoop;

    VkCommandBuffer m_cb[FRAMES_IN_FLIGHT];

    friend class AsyncFrameTest;
};

//#define TEST_ASYNC

#ifdef TEST_ASYNC
class AsyncFrameTest : public QRunnable
{
public:
    AsyncFrameTest(Worker *w) : m_w(w) { }
    void run() override { QThread::msleep(20); m_w->m_renderLoop->frameQueued(); }
    Worker *m_w;
};
#endif

#endif
