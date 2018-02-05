#include <iostream>
#include <future>
#include <fstream>

#include <string.h>

#include <gstreamermm.h>
#include <glibmm/main.h>

using namespace std;
using namespace Gst;
using Glib::RefPtr;

const size_t read_block_size = (32*1024);

static size_t getFileSize(const std::string &fileName)
{
  ifstream file(fileName.c_str(), ifstream::in | ifstream::binary);
  size_t file_size = 0;

  if(file.is_open()) {
    file.seekg(0, ios::end);
    file_size = file.tellg();
    file.close();
  }

  return file_size;
}

class PipelineContainer
{
public:

  PipelineContainer(std::string file_name) :
    fileName(file_name),
    pipeline_input(file_name, ios::in|ios::binary) {

  }

  bool Initialize()
  {
    cout << __FUNCTION__ << ": Check file" << endl;
    if (!pipeline_input.is_open()) {
      cout << __FUNCTION__ << ": Could not open input file " << endl;
      return false;
    }

    remaining_size = getFileSize(fileName);

    cout << __FUNCTION__ << ": Check file size " << remaining_size << endl;
    if (remaining_size <= 0) {
      cout << __FUNCTION__ << ": Invalid input file size " << remaining_size << endl;
      return false;
    }

    cout << __FUNCTION__ << ": Create Pipeline" << endl;
    pipeline = Pipeline::create();

    cout << __FUNCTION__ << ": Create Elements" << endl;
    video_sink = ElementFactory::create_element("appsink", "video_sink");
    audio_sink = ElementFactory::create_element("appsink", "audio_sink");
    decodebin = ElementFactory::create_element("decodebin", "decoder");
    source = ElementFactory::create_element("appsrc", "source");

    cout << __FUNCTION__ << ": Add the Elements to the Pipeline" << endl;
    pipeline->add(source)->add(decodebin)->add(video_sink)->add(audio_sink);

    cout << __FUNCTION__ << ": Link the source Element to the Decoder" << endl;
    source->link(decodebin);

    return true;
  }

  void SetUpMessageHandler(RefPtr<Glib::MainLoop> & main_loop) {
    cout << __FUNCTION__ << ": Add lambda slot for error or eos on bus" << endl;
    pipeline->get_bus()->add_watch([main_loop] (const RefPtr<Bus>&, const RefPtr<Message>& message) {
      Glib::ustring message_name = Enums::get_name(message->get_message_type());
      cout << "Message Handler: Message received: " << message_name << endl;
      switch (message->get_message_type())
      {
        case MessageType::MESSAGE_EOS:
          cout << "Message Handler: Got " << message_name << ", stopping message loop" << endl;
          main_loop->quit();
          break;
        case MessageType::MESSAGE_ERROR:
          cout << "Message Handler: Got " << message_name << ", stopping message loop" << endl;
          main_loop->quit();
          break;
        default:
          break;
      }
      return true;
    });
  }

  void printCaps(const RefPtr<Pad>& pad) {
    cout << __FUNCTION__ << " ------------------------ " << endl;

    if (pad->has_current_caps()) {

      RefPtr<Caps> caps = pad->get_current_caps();

      for (size_t i = 0; i < caps->size(); i++) {
        const Gst::Structure structure = caps->get_structure(i);
        cout << __FUNCTION__ << " Select structure at index " << i << " : " << structure.get_name() << endl;

        for (size_t j = 0; j < structure.size(); j++) {
          cout << __FUNCTION__ << " Select field at index " << j << endl;
          Glib::ustring field_name = structure.get_nth_field_name(j);
          cout << __FUNCTION__ << " Field name " << field_name << endl;
        }

        cout << __FUNCTION__ << " Structure at index " << structure.to_string() << endl;

      }

    } else {
      cout << __FUNCTION__ << " Pad has no caps" << endl;
    }

    cout << __FUNCTION__ << " ------------------------ " << endl;
  }

  bool hasStructure(const RefPtr<Pad>& pad) {
    if (!pad->has_current_caps())
      return false;
    RefPtr<Caps> caps = pad->get_current_caps();
    if (caps->size() < 1)
      return false;
    return true;
  }

  Glib::ustring getName(const RefPtr<Pad>& pad) {
    RefPtr<Caps> caps = pad->get_current_caps();
    const Gst::Structure structure = caps->get_structure(0);
    return structure.get_name();
  }

  bool isVideo(const RefPtr<Pad>& pad) {
    if (!hasStructure(pad))
      return false;
    Glib::ustring name = getName(pad);
    return name == "video/x-raw";
  }

  bool isAudio(const RefPtr<Pad>& pad) {
    if (!hasStructure(pad))
      return false;
    Glib::ustring name = getName(pad);
    return name == "audio/x-raw";
  }

  void ListenForDataNeeded() {
    cout << __FUNCTION__ << ": Add lambda slot need data on appsrc" << endl;
    cout << __FUNCTION__ << ": file size " << remaining_size << endl;

    RefPtr<AppSrc> appsrc = appsrc.cast_static(source);

    appsrc->signal_need_data().connect([&] (guint bytes_needed) {
      cout << "NeedData: bytes needed : " << bytes_needed << endl;

      size_t buf_size = min(remaining_size, size_t(bytes_needed));
      char src_buf[buf_size];

      cout << "NeedData: read " << buf_size << " remaining size " << remaining_size << endl;
      pipeline_input.read(src_buf, buf_size);

      cout << "NeedData: create buffer " << buf_size << endl;
      RefPtr<Buffer> buf = Buffer::create(buf_size);

      MapInfo map_info;
      buf->map(map_info, MAP_WRITE);
      cout << "NeedData: copy into buffer num bytes " << buf_size << endl;
      memcpy(map_info.get_data(), src_buf, buf_size);
      buf->unmap(map_info);

      RefPtr<AppSrc> appsrc = appsrc.cast_static(source);

      cout << "NeedData: push buffer with bytes " << buf_size << endl;
      FlowReturn flow = appsrc->push_buffer(buf);

      if(flow == FlowReturn::FLOW_OK)
        cout << "NeedData: Push Buffer successful" << endl;
      else
        cout << "NeedData: Push Buffer failed" << endl;

      remaining_size -= buf_size;

      cout << "NeedData: remaining size " << remaining_size << endl;
    });
  }

  void ListenForEnoughData() {
    cout << __FUNCTION__ << ": Add lambda slot enough data on appsrc" << endl;
    RefPtr<AppSrc> appsrc = appsrc.cast_static(source);
    appsrc->signal_enough_data().connect([&] () {
      cout << "EnoughData" << endl;
    });
  }


  void ListenForPads() {
    cout << __FUNCTION__ << ": Add lambda slot for new pad on decodebin" << endl;
    decodebin->signal_pad_added().connect([&] (const RefPtr<Pad>& pad) {
      cout << "PadListener: Pad added to : " << decodebin->get_name() << endl;
      cout << "PadListener: Pad added    : " << pad->get_name() << endl;

      printCaps(pad);

      PadLinkReturn ret = PadLinkReturn::PAD_LINK_REFUSED;

      if (isVideo(pad)) {
        cout << "PadListener: Trying to link video pad : " << pad->get_name() << endl;
        ret = pad->link(video_sink->get_static_pad("sink"));
      } else if (isAudio(pad)) {
        cout << "PadListener: Trying to link audio pad : " << pad->get_name() << endl;
        ret = pad->link(audio_sink->get_static_pad("sink"));
      } else {
        cout << "PadListener: Unknown pad type : " << pad->get_name() << endl;
      }

      if (ret != PadLinkReturn::PAD_LINK_OK)
        cout << "PadListener: Pads could not be linked : " << ret << endl;
      else
        cout << "PadListener: Pads linked " << endl;
    });

  }

  void StartPipeline() {
    cout << __FUNCTION__ << ": Set Pipeline State to Playing " << endl;
    pipeline->set_state(State::STATE_PLAYING);
  }

  void StopPipeline() {
    cout << __FUNCTION__ << ": Set Pipeline State to Null " << endl;
    pipeline->set_state(State::STATE_NULL);
  }

  RefPtr<Element> source;
  RefPtr<Element> decodebin;
  RefPtr<Element> audio_sink;
  RefPtr<Element> video_sink;
  RefPtr<Pipeline> pipeline;
  size_t remaining_size = 0;
  std::string fileName;
  ifstream pipeline_input;
};

int main(int argc, char *argv[])
{
  if (argc != 2) {
    cout << __FUNCTION__ << ": You need to pass in the input filename" << endl;
    return EXIT_FAILURE;
  }

  std::string fileName = argv[1];
  cout << __FUNCTION__ << ": Input file " << fileName << endl;

  cout << __FUNCTION__ << ": Initialize GStreamer" << endl;
  Gst::init(argc, argv);

  cout << __FUNCTION__ << ": Create Mainloop" << endl;
  RefPtr<Glib::MainLoop> main_loop = Glib::MainLoop::create();

  cout << __FUNCTION__ << ": Create PipelineContainer" << endl;
  PipelineContainer pipeline_container(fileName);

  cout << __FUNCTION__ << ": Initialize PipelineContainer" << endl;
  bool successful = pipeline_container.Initialize();

  if (!successful) {
    cout << __FUNCTION__ << ": Pipeline initialization failed" << endl;
    return EXIT_FAILURE;
  }

  cout << __FUNCTION__ << ": Setup Message Handling" << endl;
  pipeline_container.SetUpMessageHandler(main_loop);

  cout << __FUNCTION__ << ": Setup Listener for Pads on Decodebin" << endl;
  pipeline_container.ListenForPads();

  cout << __FUNCTION__ << ": Setup Listener for Data Needed on AppSrc" << endl;
  pipeline_container.ListenForDataNeeded();

  cout << __FUNCTION__ << ": Setup Listener for Enough Data on AppSrc" << endl;
  pipeline_container.ListenForEnoughData();

  cout << __FUNCTION__ << ": Start Pipeline" << endl;
  pipeline_container.StartPipeline();

  auto video_sink_reader_handle = async(launch::async, [&] () {
    cout << " Video Sink Reader started " << endl;

    RefPtr<AppSink> appsink = appsink.cast_static(pipeline_container.video_sink);

    size_t num_samples = 0;

    while (true) {
      RefPtr<Buffer> buf_out;
      RefPtr<Sample> sample = appsink->pull_sample();
      if (sample) {
        num_samples++;
        cout << " Video Sink Reader: Got sample number : " << num_samples << endl;
        buf_out = sample->get_buffer();
      } else {
        cout << " Video Sink Reader: No more samples, total number of samples " << num_samples << endl;
        break;
      }
    }
  });

  auto audio_sink_reader_handle = async(launch::async, [&] () {
    cout << " Audio Sink Reader started " << endl;

    RefPtr<AppSink> appsink = appsink.cast_static(pipeline_container.audio_sink);

    size_t num_samples = 0;

    while (true) {
      RefPtr<Buffer> buf_out;
      RefPtr<Sample> sample = appsink->pull_sample();
      if (sample) {
        num_samples++;
        cout << " Audio Sink Reader: Got sample number : " << num_samples << endl;
        buf_out = sample->get_buffer();
      } else {
        cout << " Audio Sink Reader: No more samples, total number of samples " << num_samples << endl;
        break;
      }
    }
  });

  cout << __FUNCTION__ << ": Start Message Loop" << endl;
  main_loop->run();

  video_sink_reader_handle.get();
  audio_sink_reader_handle.get();

  cout << __FUNCTION__ << ": Stop Pipeline" << endl;
  pipeline_container.StopPipeline();
  return EXIT_SUCCESS;
}
