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

  void Initialize()
  {
    cout << __FUNCTION__ << ": Create Pipeline" << endl;
    pipeline = Pipeline::create();

    cout << __FUNCTION__ << ": Create Elements" << endl;
    sink = ElementFactory::create_element("appsink", "sink");
    decodebin = ElementFactory::create_element("decodebin", "decoder");
    source = ElementFactory::create_element("appsrc", "source");

    cout << __FUNCTION__ << ": Add the Elements to the Pipeline" << endl;
    pipeline->add(source)->add(decodebin)->add(sink);

    cout << __FUNCTION__ << ": Link the source Element to the Decoder" << endl;
    source->link(decodebin);
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

  void ListenForPads() {
      cout << __FUNCTION__ << ": Add lambda slot for new pad on decodebin" << endl;
      decodebin->signal_pad_added().connect([&] (const RefPtr<Pad>& pad) {
          cout << "PadListener: Pad added to : " << decodebin->get_name() << endl;
          cout << "PadListener: Pad added    : " << pad->get_name() << endl;

          PadLinkReturn ret = pad->link(sink->get_static_pad("sink"));

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
  RefPtr<Element> sink;
  RefPtr<Pipeline> pipeline;
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
    PipelineContainer pipeline_container;

    cout << __FUNCTION__ << ": Initialize PipelineContainer" << endl;
    pipeline_container.Initialize();

    cout << __FUNCTION__ << ": Setup Message Handling" << endl;
    pipeline_container.SetUpMessageHandler(main_loop);

    cout << __FUNCTION__ << ": Setup Listener for Pads on Decodebin" << endl;
    pipeline_container.ListenForPads();

    cout << __FUNCTION__ << ": Start Pipeline" << endl;
    pipeline_container.StartPipeline();

  auto sink_reader_handle = async(launch::async, [&] () {
        cout << " Sink Reader started " << endl;

    RefPtr<AppSink> appsink = appsink.cast_static(pipeline_container.sink);

    size_t num_samples = 0;

    while (true) {
      RefPtr<Buffer> buf_out;
      RefPtr<Sample> sample = appsink->pull_sample();
      if (sample) {
        num_samples++;
        cout << " Sink Reader: Got sample number : " << num_samples << endl;
        buf_out = sample->get_buffer();
      } else {
        cout << " Sink Reader: No more samples, total number of samples " << num_samples << endl;
        break;
      }
    }
    });

  auto source_writer_handle = async(launch::async, [&] () {
    cout << " Source Writer: async handler started " << endl;

    ifstream pipeline_input (fileName, ios::in|ios::binary);
        if (!pipeline_input.is_open()) {
            cout << " Could not open input file " << endl;
            return;
        }

    size_t remaining_size = getFileSize(fileName);

    cout << " Source Writer: file size " << remaining_size << endl;

    FlowReturn flow = FLOW_OK;
    RefPtr<AppSrc> appsrc = appsrc.cast_static(pipeline_container.source);

    while (remaining_size > 0 && flow == FLOW_OK) {

      size_t buf_size = min(remaining_size, read_block_size);
      char src_buf[buf_size];

      cout << " Source Writer: read " << buf_size << " remaining size " << remaining_size << endl;
      pipeline_input.read(src_buf, buf_size);

      RefPtr<Buffer> buf = Buffer::create(buf_size);

      MapInfo map_info;
      buf->map(map_info, MAP_WRITE);
      memcpy(map_info.get_data(), src_buf, buf_size);
      buf->unmap(map_info);

      flow = appsrc->push_buffer(buf);
      remaining_size -= buf_size;
    }

    flow = appsrc->end_of_stream();
    });

    cout << __FUNCTION__ << ": Start Message Loop" << endl;
    main_loop->run();

    source_writer_handle.get();
    sink_reader_handle.get();

    cout << __FUNCTION__ << ": Stop Pipeline" << endl;
    pipeline_container.StopPipeline();
  return EXIT_SUCCESS;
}
