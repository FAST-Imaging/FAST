import fast
import pytest


def test_data_stream_empty_generator():
    importer = fast.WholeSlideImageImporter.create(fast.Config.getTestDataPath() + '/WSI/CMU-1.svs')

    segmentation = fast.TissueSegmentation.create(True).connect(importer).runAndGetOutputData()
    segmentation.fill(0) # Empty segmentation, means no patches
    generator = fast.PatchGenerator.create(256, 256, level=2).connect(importer).connect(1, segmentation)

    counter = 0
    with pytest.raises(RuntimeError):
        for image in fast.DataStream(generator):
            counter += 1
    assert counter == 0


def test_data_stream_empty_file_streamer():
    streamer = fast.ImageFileStreamer \
        .create(fast.Config.getTestDataPath() + "/US/Heart/ApicalFourChamber/US-2D_#.mhd",)
    streamer.setStartNumber(200) # Incorrect start number, will result in no frames

    counter = 0
    with pytest.raises(RuntimeError):
        for image in fast.DataStream(streamer):
            counter += 1
    assert counter == 0


def test_data_stream_single():
    streamer = fast.ImageFileStreamer\
        .create(fast.Config.getTestDataPath() + "/US/Heart/ApicalFourChamber/US-2D_#.mhd")

    dataStream = fast.DataStream(streamer)
    previousImage = ''
    counter = 0
    for image in dataStream:
        assert previousImage != image
        assert image.getWidth() == 234
        assert image.getHeight() == 591
        previousImage = image
        counter += 1
    assert counter == 100


def test_data_stream_multiple():
    streamer1 = fast.ImageFileStreamer \
        .create(fast.Config.getTestDataPath() + "/US/Heart/ApicalFourChamber/US-2D_#.mhd")
    streamer2 = fast.ImageFileStreamer \
        .create(fast.Config.getTestDataPath() + "/US/Heart/ApicalTwoChamber/US-2D_#.mhd")
    streamer3 = fast.ImageFileStreamer \
        .create(fast.Config.getTestDataPath() + "/US/Heart/ApicalLongAxis/US-2D_#.mhd")

    dataStream = fast.DataStream(streamer1, streamer2, streamer3)
    previousImage1 = ''
    previousImage2 = ''
    previousImage3 = ''
    counter = 0
    for image1, image2, image3 in dataStream:
        assert previousImage1 != image1
        assert previousImage2 != image2
        assert previousImage3 != image3
        assert image1.getWidth() == 234
        assert image1.getHeight() == 591
        assert image2.getWidth() == 234
        assert image2.getHeight() == 591
        assert image3.getWidth() == 234
        assert image3.getHeight() == 591
        previousImage1 = image1
        previousImage2 = image2
        previousImage3 = image3
        counter += 1
    assert counter == 100
