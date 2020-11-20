"""
This example will stream images from your webcamera and run it through a simple edge detection filter (LaplacianOfGaussian)
and display it in real-time.
"""
import fast

streamer = fast.CameraStreamer.New()

filter = fast.LaplacianOfGaussian.New()
filter.setInputConnection(streamer.getOutputPort())

renderer = fast.ImageRenderer.New()
renderer.addInputConnection(filter.getOutputPort())

window = fast.SimpleWindow.New()
window.addRenderer(renderer)
window.set2DMode()
window.start()
