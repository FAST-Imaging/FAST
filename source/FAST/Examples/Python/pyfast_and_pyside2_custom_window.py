## @example pyfast_and_pyside2_custom_window.py
# This example demonstrates how to use FAST in an existing PySide2 application.
#
# @m_class{m-block m-warning} @par PySide2 Qt Version
# @parblock
#     For this example you <b>must</b> use the same Qt version of PySide2 as used in FAST (5.15.2)
#     Do this with: <b>pip install pyside2==5.15.2.1</b>
# @endparblock
#
# @image html images/examples/python/pyfast_and_pyside_custom_window.jpg width=350px;

import platform
if platform.system() != 'Windows':
    import PySide2.QtSvg # Must import this before fast due to conflicting symbols
import fast # Must import FAST before rest of pyside2
from PySide2.QtWidgets import *
from PySide2.QtCore import Slot
from shiboken2 import wrapInstance

#fast.Reporter.setGlobalReportMethod(fast.Reporter.COUT)

# Create a simple window widget with pyside2
class Window(QWidget):
    def __init__(self):
        super(Window, self).__init__()
        self.setWindowTitle('pyFAST + PySide2')

        # Create button
        self.button = QPushButton("Restart FAST pipeline")

        # Create FAST view
        self.view = fast.View()
        self.installEventFilter(wrapInstance(int(self.view.asQGLWidget()), QOpenGLWidget))
        self.view.set2DMode()

        # Create layout and add widgets
        layout = QVBoxLayout()
        layout.addWidget(wrapInstance(int(self.view.asQGLWidget()), QOpenGLWidget))
        layout.addWidget(self.button)
        self.setLayout(layout)

        # Connect button click event
        self.button.clicked.connect(self.restartPipeline)

        self.resize(512, 512)

    @Slot()
    def restartPipeline(self):
        # Create FAST computation thread
        # This is needed to run computations smoothly in the background
        # The computation thread must live in the object to avoid being destroyed when this function is done.
        self.computationThread = fast.ComputationThread.create()
        self.computationThread.addView(self.view)

        # Setup a FAST pipeline
        streamer = fast.ImageFileStreamer \
            .create(fast.Config.getTestDataPath() + '/US/Heart/ApicalFourChamber/US-2D_#.mhd', framerate=25)

        renderer = fast.ImageRenderer.create() \
            .connect(streamer)

        self.view.removeAllRenderers()
        self.view.addRenderer(renderer)
        self.view.reinitialize()
        self.computationThread.start()


if __name__ == '__main__':
    # Get the Qt Application
    app = QApplication.instance()

    # Create and show the window
    window = Window()
    window.show()

    # Run the main Qt loop
    app.exec_()
