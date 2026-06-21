#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <format>
#include <glad/gl.h>
#include <glm/ext/vector_float2.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string_view>
#include <vector>

class ScreenWindow {
public:
  ScreenWindow() {
    if (!SDL_Init(SDL_INIT_VIDEO))
      throw std::runtime_error("Failed to initialize SDL video.");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_PropertiesID props{SDL_CreateProperties()};
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                          title_.data());
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, size_.x);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, size_.y);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN,
                           true);

    window_ = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);

    if (!window_) {
      SDL_Quit();
      throw std::runtime_error(
          std::format("Failed to create window: {}", SDL_GetError()));
    }

    glContext_ = SDL_GL_CreateContext(window_);
    if (!glContext_) {
      SDL_DestroyWindow(window_);
      SDL_Quit();
      throw std::runtime_error(
          std::format("Failed to create opengl context: {}", SDL_GetError()));
    }

    SDL_GL_MakeCurrent(window_, glContext_);

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
      SDL_GL_DestroyContext(glContext_);
      SDL_DestroyWindow(window_);
      SDL_Quit();
      throw std::runtime_error("Failed to initialize glad.");
    }

    SDL_GL_SetSwapInterval(0);
    refreshViewport();
  }

  ~ScreenWindow() {
    if (glContext_)
      SDL_GL_DestroyContext(glContext_);

    if (window_)
      SDL_DestroyWindow(window_);

    SDL_Quit();
  }

  ScreenWindow(ScreenWindow &) = delete;
  ScreenWindow &operator=(ScreenWindow &) = delete;

  void refreshViewport() { glViewport(0, 0, size_.x, size_.y); }

  glm::ivec2 size() const { return size_; }

  void setSize(glm::ivec2 newSize) {
    size_ = newSize;
    refreshViewport();
  }

  bool shouldClose() const { return shouldClose_; }

  void close() { shouldClose_ = true; }

  SDL_Window *getWindow() const { return window_; }

private:
  SDL_Window *window_{nullptr};
  SDL_GLContext glContext_{nullptr};

  glm::ivec2 size_{800, 800};
  std::string_view title_{"Sorting visualizer"};
  bool shouldClose_{false};
};

std::string_view vertexShaderSource{R"GLSL(
  #version 460 core

  layout (location=0) in vec2 aPosition;

  uniform vec2 uViewport;
  
  void main(){
    vec2 normalizedPosition = aPosition / uViewport;
    vec2 clipPosition = vec2(
      normalizedPosition.x * 2.0 - 1.0,
      1.0 - normalizedPosition.y * 2.0
    );

    gl_Position = vec4(clipPosition, 0, 1);
  }
)GLSL"};

std::string_view fragmentShaderSource{R"GLSL(
  #version 460 core

  out vec4 fragColor;

  void main(){
    fragColor = vec4(1);
  }

)GLSL"};

class VisualizerRenderer {
public:
  VisualizerRenderer() {

    glGenVertexArrays(1, &vertexArray_);
    glGenBuffers(1, &vertexBuffer_);

    glBindVertexArray(vertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2),
                          (void *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    shaderProgram_ = createProgram();

    uniformViewportLocation = glGetUniformLocation(shaderProgram_, "uViewport");
    uniformModelLocation = glGetUniformLocation(shaderProgram_, "uModel");
  }

  ~VisualizerRenderer() {
    if (shaderProgram_)
      glDeleteProgram(shaderProgram_);

    if (vertexArray_)
      glDeleteVertexArrays(1, &vertexArray_);

    if (vertexBuffer_)
      glDeleteBuffers(1, &vertexBuffer_);
  }

  VisualizerRenderer(const VisualizerRenderer &) = delete;
  VisualizerRenderer &operator=(VisualizerRenderer &) = delete;

  template <typename TNumber>
  void renderDiagram(const std::vector<TNumber> &data,
                     const glm::ivec2 windowSize) {

    if (data.empty())
      return;

    const size_t N{data.size()};

    std::vector<glm::vec2> vertexData(N * 6);

    const float barWidth{windowSize.x / static_cast<float>(N)};
    const TNumber maxValue{*std::max_element(data.begin(), data.end())};
    const float scale{windowSize.y / static_cast<float>(maxValue)};

    for (size_t i{0}; i < N; i++) {
      const float x{i * barWidth};
      const float barHeight{data[i] * scale};
      const float bottom{static_cast<float>(windowSize.y)};
      const float y = {bottom - barHeight};

      const size_t baseIndex{i * 6};
      vertexData[baseIndex] = glm::vec2(x, bottom);
      vertexData[baseIndex + 1] = glm::vec2(x + barWidth, bottom);
      vertexData[baseIndex + 2] = glm::vec2(x + barWidth, y);

      vertexData[baseIndex + 3] = glm::vec2(x, bottom);
      vertexData[baseIndex + 4] = glm::vec2(x + barWidth, y);
      vertexData[baseIndex + 5] = glm::vec2(x, y);
    }

    glUseProgram(shaderProgram_);
    glUniform2f(uniformViewportLocation, static_cast<float>(windowSize.x),
                static_cast<float>(windowSize.y));

    glBindVertexArray(vertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(glm::vec2),
                 vertexData.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexData.size()));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glUseProgram(0);
  }

private:
  GLuint createProgram() const {
    GLuint vertexShader{
        compileShader(GL_VERTEX_SHADER, vertexShaderSource.data())};

    GLuint fragmentShader{
        compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource.data())};

    GLuint shaderProgram{glCreateProgram()};
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
  }

  GLuint compileShader(GLenum type, const char *source) const {
    GLuint shader{glCreateShader(type)};
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
  }

  GLuint vertexArray_{0};
  GLuint vertexBuffer_{0};
  GLuint elementBuffer_{0};
  GLuint shaderProgram_{0};
  GLint uniformViewportLocation{0};
  GLint uniformModelLocation{0};
};

template <typename TNumber> class StepSorting {
public:
  virtual ~StepSorting() = default;

  virtual void step() = 0;

  void setData(const std::vector<TNumber> &data) {
    data_ = data;
    N_ = data_.size();
    isDone_ = false;
    wasSwap_ = false;
    reset();
  }

  const std::vector<TNumber> &getData() { return data_; }

  bool wasSwap() const { return wasSwap_; }

  bool isDone() const { return isDone_; }

protected:
  virtual void reset() = 0;

  std::vector<TNumber> data_;
  bool isDone_{false};
  size_t N_{0};
  bool wasSwap_{false};
};

template <typename TNumber> class BubbleSort : public StepSorting<TNumber> {
public:
  void step() override {

    this->wasSwap_ = false;

    if (this->isDone_ || this->N_ == 0)
      return;

    if (i_ < this->N_) {
      if (j_ < this->N_ - i_ - 1) {
        if (this->data_[j_ + 1] < this->data_[j_]) {
          std::swap(this->data_[j_ + 1], this->data_[j_]);
          this->wasSwap_ = true;
          j_++;
          return;
        }
        j_++;
      } else {
        i_++;
        j_ = 0;
      }
    } else {
      this->isDone_ = true;
    }
  }

private:
  void reset() override {
    i_ = 0;
    j_ = 0;
  }

  size_t i_{0};
  size_t j_{0};
};

template <typename TNumber> class InsertionSort : public StepSorting<TNumber> {
public:
  void step() override {
    this->wasSwap_ = false;

    if (this->isDone_ || this->N_ == 0)
      return;

    if (i_ < this->N_) {
      if (j_ > 0 && this->data_[j_] < this->data_[j_ - 1]) {
        std::swap(this->data_[j_], this->data_[j_ - 1]);
        this->wasSwap_ = true;
        j_--;
        return;
      } else {
        i_++;
        j_ = i_;
      }

    } else {
      this->isDone_ = true;
    }
  }

private:
  void reset() override {
    i_ = 1;
    j_ = 1;
  }

  size_t i_{0};
  size_t j_{0};
};

template <typename TNumber> class SelectionSort : public StepSorting<TNumber> {
public:
  void step() override {

    this->wasSwap_ = false;

    if (this->isDone_ || this->N_ == 0)
      return;

    if (i_ < this->N_ - 1) {
      if (this->data_[j_] < this->data_[minIndex_]) {
        minIndex_ = j_;
      }

      if (j_ == this->N_ - 1) {
        if (minIndex_ != i_) {
          std::swap(this->data_[i_], this->data_[minIndex_]);
          this->wasSwap_ = true;
        }
        i_++;
        minIndex_ = j_ = i_;
      } else {
        j_++;
      }

    } else {
      this->isDone_ = true;
    }
  }

private:
  void reset() override {
    i_ = 0;
    j_ = 0;
    minIndex_ = 0;
  }

  size_t i_{0};
  size_t j_{0};
  size_t minIndex_{0};
};

template <typename TNumber> class SortingVisualizer {
public:
  SortingVisualizer() {
    data_.resize(s_elementCount);
    std::iota(data_.begin(), data_.end(), 1);

    std::mt19937 generator{std::random_device{}()};
    std::shuffle(data_.begin(), data_.end(), generator);
  }

  SortingVisualizer(const SortingVisualizer &) = delete;
  SortingVisualizer &operator=(const SortingVisualizer &) = delete;

  void update(uint64_t dt) {

    if (!sortAlgorithm_ || sortAlgorithm_->isDone()) {
      return;
    }

    accumulator_ += dt;
    if (accumulator_ < interval_) {
      return;
    }
    accumulator_ = 0;

    while (!sortAlgorithm_->isDone()) {
      sortAlgorithm_->step();
      if (sortAlgorithm_->wasSwap())
        break;
    }
  }

  const std::vector<TNumber> &getActualData() const {
    if (!sortAlgorithm_) {
      static const std::vector<TNumber> empty;
      return empty;
    }

    return sortAlgorithm_->getData();
  }
  const std::vector<int> &getBeginData() const { return data_; }

  template <typename TAlgorithm> void selectSortAlgorithm() {
    sortAlgorithm_ = std::make_unique<TAlgorithm>();
    sortAlgorithm_->setData(data_);
  }

  void resetSortData() {
    if (!sortAlgorithm_)
      return;

    sortAlgorithm_->setData(data_);
  }

  bool isSorted() const {
    if (!sortAlgorithm_)
      return false;

    return sortAlgorithm_->isDone();
  }

private:
  std::unique_ptr<StepSorting<TNumber>> sortAlgorithm_;
  std::vector<TNumber> data_{};
  uint64_t accumulator_{0};
  uint64_t interval_{0};
  inline static constexpr size_t s_elementCount{300};
};

int main(int argc, char *argv[]) {

  using Number = int;

  try {
    ScreenWindow screenWindow;
    VisualizerRenderer visualizerRenderer;
    SortingVisualizer<Number> sortingVisualizer;

    sortingVisualizer.selectSortAlgorithm<BubbleSort<Number>>();

    uint64_t lastTime{SDL_GetTicks()};
    uint64_t beginSortTime{SDL_GetTicks()};

    glClearColor(0, 0, 0, 1);
    while (!screenWindow.shouldClose()) {
      glClear(GL_COLOR_BUFFER_BIT);

      uint64_t currentTime{SDL_GetTicks()};
      uint64_t dt{currentTime - lastTime};
      lastTime = currentTime;

      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
          screenWindow.close();
        }

        if (event.type == SDL_EVENT_WINDOW_RESIZED) {
          screenWindow.setSize({event.window.data1, event.window.data2});
        }
      }

      sortingVisualizer.update(dt);

      if (sortingVisualizer.isSorted()) {
        static bool first{true};
        if (first) {
          float sortTime{static_cast<float>(SDL_GetTicks() - beginSortTime) /
                         1000.f};
          SDL_Log("sort time: %.02fs", sortTime);
          first = false;
        }
      }

      visualizerRenderer.renderDiagram(sortingVisualizer.getActualData(),
                                       screenWindow.size());

      SDL_GL_SwapWindow(screenWindow.getWindow());
    }

  } catch (const std::exception &e) {
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Critical error: %s",
                    e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
