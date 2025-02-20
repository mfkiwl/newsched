#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# SPDX-License-Identifier: GPL-3.0
#
# GNU Radio Python Flow Graph
# Title: Not titled yet
# Author: josh
# GNU Radio version: joshtest-330-ga6fbb06a

from distutils.version import StrictVersion

if __name__ == '__main__':
    import ctypes
    import sys
    if sys.platform.startswith('linux'):
        try:
            x11 = ctypes.cdll.LoadLibrary('libX11.so')
            x11.XInitThreads()
        except:
            print("Warning: failed to XInitThreads()")

from PyQt5 import Qt
from newsched import qtgui, soapy, fft
import sip
from newsched import blocks
from newsched import gr
import sys
import signal


class test_qt(Qt.QWidget):
    def start(self):
        self.fg.start()

    def stop(self):
        self.fg.stop()
        
    def wait(self):
        self.fg.wait()
        
    def __init__(self):
        # gr.flowgraph.__init__(self)
        self.fg = gr.flowgraph()
        Qt.QWidget.__init__(self)
        self.setWindowTitle("Not titled yet")
        # qtgui.util.check_set_qss()
        try:
            self.setWindowIcon(Qt.QIcon.fromTheme('gnuradio-grc'))
        except:
            pass
        self.top_scroll_layout = Qt.QVBoxLayout()
        self.setLayout(self.top_scroll_layout)
        self.top_scroll = Qt.QScrollArea()
        self.top_scroll.setFrameStyle(Qt.QFrame.NoFrame)
        self.top_scroll_layout.addWidget(self.top_scroll)
        self.top_scroll.setWidgetResizable(True)
        self.top_widget = Qt.QWidget()
        self.top_scroll.setWidget(self.top_widget)
        self.top_layout = Qt.QVBoxLayout(self.top_widget)
        self.top_grid_layout = Qt.QGridLayout()
        self.top_layout.addLayout(self.top_grid_layout)

        self.settings = Qt.QSettings("GNU Radio", "qttest")

        try:
            if StrictVersion(Qt.qVersion()) < StrictVersion("5.0.0"):
                self.restoreGeometry(self.settings.value("geometry").toByteArray())
            else:
                self.restoreGeometry(self.settings.value("geometry"))
        except:
            pass

        ##################################################
        # Variables
        ##################################################
        self.samp_rate = samp_rate = 250000

        ##################################################
        # Blocks
        ##################################################
        self.tsnk = qtgui.time_sink_c(
            1024, #size
            samp_rate, #samp_rate
            "", #name
            1 #number of inputs
        )

        '''
        self.tsnk.set_update_time(0.10)
        self.tsnk.set_y_axis(-1, 1)

        self.tsnk.set_y_label('Amplitude', "")

        self.tsnk.enable_tags(True)
        self.tsnk.set_trigger_mode(qtgui.TRIG_MODE_FREE, qtgui.TRIG_SLOPE_POS, 0.0, 0, 0, "")
        self.tsnk.enable_autoscale(False)
        self.tsnk.enable_grid(False)
        self.tsnk.enable_axis_labels(True)
        self.tsnk.enable_control_panel(False)
        self.tsnk.enable_stem_plot(False)


        labels = ['Signal 1', 'Signal 2', 'Signal 3', 'Signal 4', 'Signal 5',
            'Signal 6', 'Signal 7', 'Signal 8', 'Signal 9', 'Signal 10']
        widths = [1, 1, 1, 1, 1,
            1, 1, 1, 1, 1]
        colors = ['blue', 'red', 'green', 'black', 'cyan',
            'magenta', 'yellow', 'dark red', 'dark green', 'dark blue']
        alphas = [1.0, 1.0, 1.0, 1.0, 1.0,
            1.0, 1.0, 1.0, 1.0, 1.0]
        styles = [1, 1, 1, 1, 1,
            1, 1, 1, 1, 1]
        markers = [-1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1]


        for i in range(1):
            if len(labels[i]) == 0:
                self.tsnk.set_line_label(i, "Data {0}".format(i))
            else:
                self.tsnk.set_line_label(i, labels[i])
            self.tsnk.set_line_width(i, widths[i])
            self.tsnk.set_line_color(i, colors[i])
            self.tsnk.set_line_style(i, styles[i])
            self.tsnk.set_line_marker(i, markers[i])
            self.tsnk.set_line_alpha(i, alphas[i])
        '''
        print(self.tsnk.qwidget())

        self._tsnk_win = sip.wrapinstance(self.tsnk.qwidget(), Qt.QWidget)
        self.top_layout.addWidget(self._tsnk_win)


        self.fsnk = qtgui.freq_sink_c(
            1024, #size
            fft.window.WIN_BLACKMAN_hARRIS, #wintype
            0, #fc
            samp_rate, #bw
            "", #name
            1,
        )
        self.fsnk.set_update_time(0.10)
        self.fsnk.set_y_axis(-140, 10)
        self.fsnk.set_y_label('Relative Gain', 'dB')
        # self.fsnk.set_trigger_mode(qtgui.TRIG_MODE_FREE, 0.0, 0, "")
        self.fsnk.enable_autoscale(False)
        self.fsnk.enable_grid(False)
        self.fsnk.set_fft_average(1.0)
        self.fsnk.enable_axis_labels(True)
        self.fsnk.enable_control_panel(False)
        self.fsnk.set_fft_window_normalized(False)

        self._fsnk_win = sip.wrapinstance(self.fsnk.qwidget(), Qt.QWidget)
        self.top_layout.addWidget(self._fsnk_win)

        # src = blocks.vector_source_f([x / 100 for x in range(0,100) ], True, 1, [])
        src = soapy.source_cc('driver=rtlsdr',1)
        src.set_sample_rate(0, samp_rate)
        src.set_gain_mode(0, False)
        src.set_frequency(0, 90500000)
        src.set_frequency_correction(0, 0)
        src.set_gain(0, 'TUNER', 42)


        ##################################################
        # Connections
        ##################################################
        self.fg.connect(src, 0, self.tsnk, 0)
        self.fg.connect(src, 0, self.fsnk, 0)


    def closeEvent(self, event):
        self.settings = Qt.QSettings("GNU Radio", "qttest")
        self.settings.setValue("geometry", self.saveGeometry())
        self.stop()
        self.wait()

        event.accept()

    def get_samp_rate(self):
        return self.samp_rate

    def set_samp_rate(self, samp_rate):
        self.samp_rate = samp_rate
        self.tsnk.set_samp_rate(self.samp_rate)




def main(top_block_cls=test_qt, options=None):

    if StrictVersion("4.5.0") <= StrictVersion(Qt.qVersion()) < StrictVersion("5.0.0"):
        style = gr.prefs().get_string('qtgui', 'style', 'raster')
        Qt.QApplication.setGraphicsSystem(style)
    qapp = Qt.QApplication(sys.argv)

    tb = top_block_cls()

    tb.start()

    tb.show()

    def sig_handler(sig=None, frame=None):
        tb.stop()
        tb.wait()

        Qt.QApplication.quit()

    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)

    timer = Qt.QTimer()
    timer.start(500)
    timer.timeout.connect(lambda: None)

    qapp.exec_()

if __name__ == '__main__':
    main()
