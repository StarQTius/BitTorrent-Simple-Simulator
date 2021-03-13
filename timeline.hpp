#ifndef H_TIMELINE
  #define H_TIMELINE

  #include <cstdint>
  #include <exception>
  #include <queue>
  #include <utility>

  template<typename D, typename ...Args>
  class Timeline{
    public:
      /**Definitions**/
      typedef long double timestamp;
      typedef void (process)(Args..., Timeline<D, Args...>&, D&&);

      class Event{
        public:
          /**Definitions**/
          class Cmp{
            public:
              /**Methods**/
              bool operator()(const Event& a, const Event& b){
                return a.date_occ > b.date_occ;
              }
          };

          /**Methods**/
          Event(timestamp, const D&, process&);
          void resolve(Args..., Timeline<D, Args...>&);
          timestamp getDate() const;

        private:
          /**Attributes**/
          timestamp date_occ;
          D data;
          process* on_event;
      };

      class Prior_To_Date : public std::exception{
        public:
          /**Methods**/
          virtual const char* what() const noexcept{
            return "Attempted to insert an event prior to the current date";
          }
      };

      class No_Event : public std::exception{
        public:
          /**Methods**/
          virtual const char* what() const noexcept{
            return "Attempted to forward while there is no upcoming event";
          }
      };

      /**Methods**/
      void pushEvent(timestamp, const D&, process&);
      void forward(Args...);
      timestamp getDate() const;
      bool isStable() const;

    private:
      /**Attributes**/
      timestamp date = 0;
      std::priority_queue<Event, std::vector<Event>, typename Event::Cmp> pqueue;
  };

  #include "timeline.tcc"

#endif
