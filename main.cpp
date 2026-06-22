#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_surface.h>
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
  layout (location=1) in float aColorFlag;

  out float colorFlag;

  uniform vec2 uViewport;

  void main(){
    colorFlag = aColorFlag;

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

  in float colorFlag;

  out vec4 fragColor;

  void main(){
    vec3 color = vec3(1);
    
    if(colorFlag > 1.5)
      color = vec3(1, 0, 0);
    else if(colorFlag > 0.5)
      color = vec3(0, 1, 0);

    fragColor = vec4(color, 1);
  }

)GLSL"};

struct SortingState {
  bool isDone{false};
  bool wasSwap{false};
  glm::uvec2 swappedIndices{0, 0};
};

class VisualizerRenderer {
public:
  VisualizerRenderer() {
    glGenVertexArrays(1, &vertexArray_);
    glGenBuffers(1, &vertexBuffer_);

    glBindVertexArray(vertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec3),
                          (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(glm::vec3),
                          (void *)(sizeof(glm::vec2)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    createProgram();

    uniformViewportLocation_ =
        glGetUniformLocation(shaderProgram_, "uViewport");
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
                     const glm::ivec2 windowSize,
                     const SortingState &sortingState) {
    if (data.empty())
      return;

    const size_t N{data.size()};
    vertexData_.resize(N * 6);

    const float barWidth{windowSize.x / static_cast<float>(N)};
    const TNumber maxValue{*std::max_element(data.begin(), data.end())};
    const float scale{windowSize.y / static_cast<float>(maxValue)};

    for (size_t i{0}; i < N; i++) {
      float value = data[i];

      if (sortingState.wasSwap) {
        if (i == sortingState.swappedIndices.x)
          value = data[sortingState.swappedIndices.y];
        else if (i == sortingState.swappedIndices.y)
          value = data[sortingState.swappedIndices.x];
      }

      const float x{i * barWidth};
      const float barHeight{value * scale};
      const float bottom{static_cast<float>(windowSize.y)};
      const float y{bottom - barHeight};

      float colorFlag{0};
      if (sortingState.isDone) {
        colorFlag = 1;
      } else if (sortingState.wasSwap) {
        if (i == sortingState.swappedIndices.x ||
            i == sortingState.swappedIndices.y)
          colorFlag = 2;
      }

      const size_t baseIndex{i * 6};
      vertexData_[baseIndex] = glm::vec3(x, bottom, colorFlag);
      vertexData_[baseIndex + 1] = glm::vec3(x + barWidth, bottom, colorFlag);
      vertexData_[baseIndex + 2] = glm::vec3(x + barWidth, y, colorFlag);

      vertexData_[baseIndex + 3] = glm::vec3(x, bottom, colorFlag);
      vertexData_[baseIndex + 4] = glm::vec3(x + barWidth, y, colorFlag);
      vertexData_[baseIndex + 5] = glm::vec3(x, y, colorFlag);
    }

    glUseProgram(shaderProgram_);
    glUniform2f(uniformViewportLocation_, static_cast<float>(windowSize.x),
                static_cast<float>(windowSize.y));

    glBindVertexArray(vertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

    glBufferData(GL_ARRAY_BUFFER, vertexData_.size() * sizeof(glm::vec3),
                 vertexData_.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexData_.size()));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glUseProgram(0);
  }

private:
  void createProgram() {
    GLuint vertexShader{
        compileShader(GL_VERTEX_SHADER, vertexShaderSource.data())};

    GLuint fragmentShader{
        compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource.data())};

    shaderProgram_ = glCreateProgram();
    glAttachShader(shaderProgram_, vertexShader);
    glAttachShader(shaderProgram_, fragmentShader);
    glLinkProgram(shaderProgram_);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
  }

  GLuint compileShader(GLenum type, const char *source) const {
    GLuint shader{glCreateShader(type)};
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
  }

  std::vector<glm::vec3> vertexData_;
  GLuint vertexArray_{0};
  GLuint vertexBuffer_{0};

  GLuint shaderProgram_{0};
  GLint uniformViewportLocation_{0};
};

template <typename TNumber> class StepSorting {
public:
  virtual void step() = 0;

  virtual ~StepSorting() = default;

  void setData(const std::vector<TNumber> &data) {
    data_ = data;
    N_ = data_.size();
    isDone_ = false;
    wasSwap_ = false;
    reset();
  }

  const std::vector<TNumber> &getData() { return data_; }

  bool wasSwap() const { return wasSwap_; }

  glm::uvec2 getSwappedIndices() const { return swappedIndices_; }

  bool isDone() const { return isDone_; }

protected:
  virtual void reset() = 0;

  std::vector<TNumber> data_;
  size_t N_{0};
  bool wasSwap_{false};
  glm::uvec2 swappedIndices_{0, 0};
  bool isDone_{false};
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
          this->swappedIndices_ = {j_ + 1, j_};
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
        this->swappedIndices_ = {j_, j_ - 1};
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
          this->swappedIndices_ = {i_, minIndex_};
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
    data_.resize(kElementCount);
    std::iota(data_.begin(), data_.end(), 1);

    std::mt19937 generator{std::random_device{}()};
    std::shuffle(data_.begin(), data_.end(), generator);
  }

  SortingVisualizer(const SortingVisualizer &) = delete;
  SortingVisualizer &operator=(const SortingVisualizer &) = delete;

  void update(uint64_t dt) {
    if (!sortAlgorithm_ || sortAlgorithm_->isDone())
      return;

    passedTime_ += dt;
    if (passedTime_ < kInterval)
      return;

    passedTime_ = 0;
    sortingState_ = {};

    while (!sortAlgorithm_->isDone()) {
      sortAlgorithm_->step();
      if (sortAlgorithm_->wasSwap()) {
        sortingState_.wasSwap = true;
        sortingState_.swappedIndices = sortAlgorithm_->getSwappedIndices();
        return;
      }
    }

    sortingState_.isDone = true;
  }

  const std::vector<TNumber> &getData() const {
    if (!sortAlgorithm_)
      return data_;

    return sortAlgorithm_->getData();
  }

  template <typename TAlgorithm> void selectSortAlgorithm() {
    sortAlgorithm_ = std::make_unique<TAlgorithm>();
    reset();
  }

  void resetSortData() {
    if (!sortAlgorithm_)
      return;
    reset();
  }

  const SortingState &getSortingState() const { return sortingState_; }

  bool oneFrameAfterSortingDone() {
    if (usedFrameAfterSortingDone_)
      return false;

    if (sortingState_.isDone) {
      usedFrameAfterSortingDone_ = true;
      endSortingTime_ = SDL_GetTicks();
      return true;
    }

    return false;
  }

  size_t getSortingTime() const { return endSortingTime_ - beginSortingTime_; }

private:
  void reset() {
    sortingState_ = {};
    passedTime_ = 0;
    usedFrameAfterSortingDone_ = false;
    sortAlgorithm_->setData(data_);
    beginSortingTime_ = SDL_GetTicks();
  }

  std::unique_ptr<StepSorting<TNumber>> sortAlgorithm_;
  std::vector<TNumber> data_{};
  uint64_t passedTime_{0};
  SortingState sortingState_{};
  bool usedFrameAfterSortingDone_{false};
  size_t beginSortingTime_{0};
  size_t endSortingTime_{0};

  static constexpr uint64_t kInterval{50};
  static constexpr size_t kElementCount{30};
};

class Controller {
public:
  void processInput(const SDL_Event &event) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN: {
      keys_[event.key.key] = true;
      break;
    }

    case SDL_EVENT_KEY_UP: {
      keys_[event.key.key] = false;
      break;
    }
    }
  }

  bool isKeyPressed(SDL_Keycode key) const {
    auto it{keys_.find(key)};
    if (it != keys_.end())
      return it->second;
    return false;
  }

private:
  std::unordered_map<SDL_Keycode, bool> keys_;
};

class SortingVisualizerManager {
public:
  template <typename TNumber>
  void update(SortingVisualizer<TNumber> &sortingVisualizer,
              const Controller &controller) {

    if (controller.isKeyPressed(SDLK_1)) {
      isKey1Down_ = true;
    } else if (!controller.isKeyPressed(SDLK_1) && isKey1Down_) {
      isKey1Down_ = false;
      sortingVisualizer.template selectSortAlgorithm<BubbleSort<TNumber>>();
      SDL_Log("Selected bubble sort");
    }

    if (controller.isKeyPressed(SDLK_2)) {
      isKey2Down_ = true;
    } else if (!controller.isKeyPressed(SDLK_2) && isKey2Down_) {
      isKey2Down_ = false;
      sortingVisualizer.template selectSortAlgorithm<InsertionSort<TNumber>>();
      SDL_Log("Selected insertion sort");
    }

    if (controller.isKeyPressed(SDLK_3)) {
      isKey3Down_ = true;
    } else if (!controller.isKeyPressed(SDLK_3) && isKey3Down_) {
      isKey3Down_ = false;
      sortingVisualizer.template selectSortAlgorithm<SelectionSort<TNumber>>();
      SDL_Log("Selected selection sort");
    }

    if (controller.isKeyPressed(SDLK_SPACE)) {
      isSpaceDown_ = true;
    } else if (!controller.isKeyPressed(SDLK_SPACE) && isSpaceDown_) {
      isSpaceDown_ = false;
      sortingVisualizer.resetSortData();
      SDL_Log("Reset sort data");
    }
  }

private:
  bool isKey1Down_{false};
  bool isKey2Down_{false};
  bool isKey3Down_{false};
  bool isSpaceDown_{false};
};

int main(int argc, char *argv[]) {
  using Number = int;

  try {
    ScreenWindow screenWindow;
    VisualizerRenderer visualizerRenderer;
    SortingVisualizer<Number> sortingVisualizer;
    SortingVisualizerManager sortingVisualizerManager;
    Controller controller;

    sortingVisualizer.selectSortAlgorithm<BubbleSort<Number>>();

    uint64_t lastTime{SDL_GetTicks()};

    bool isDirty{true};
    glClearColor(0, 0, 0, 1);

    while (!screenWindow.shouldClose()) {
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
          isDirty = true;
        }

        controller.processInput(event);
        if (controller.isKeyPressed(SDLK_ESCAPE))
          screenWindow.close();
      }

      sortingVisualizerManager.update(sortingVisualizer, controller);

      sortingVisualizer.update(dt);
      const auto &sortingState{sortingVisualizer.getSortingState()};
      isDirty = isDirty || sortingState.wasSwap;

      if (sortingVisualizer.oneFrameAfterSortingDone()) {
        isDirty = true;
        SDL_Log("Sorting time: %.02fs",
                static_cast<float>(sortingVisualizer.getSortingTime()) /
                    1000.f);
      }

      if (isDirty) {
        isDirty = false;
        glClear(GL_COLOR_BUFFER_BIT);

        visualizerRenderer.renderDiagram(sortingVisualizer.getData(),
                                         screenWindow.size(), sortingState);

        SDL_GL_SwapWindow(screenWindow.getWindow());
      }
    }

  } catch (const std::exception &e) {
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Critical error: %s",
                    e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
