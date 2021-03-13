#include "timeline.hpp"

/*******************************************************************************
  Timeline
*******************************************************************************/

template<typename D, typename ...Args>
void Timeline<D, Args...>::pushEvent
  (Timeline::timestamp date_occ, const D& data,
   Timeline<D, Args...>::process& on_event)
{
  if(date_occ < date){
    throw Prior_To_Date();
  }

  pqueue.push(Event(date_occ, data, on_event));
}

template<typename D, typename ...Args>
void Timeline<D, Args...>::forward(Args... args){
  if(isStable()){
    throw No_Event();
  }

  Event event = pqueue.top();
  pqueue.pop();
  date = event.getDate();
  event.resolve(args..., *this);
}

template<typename D, typename ...Args>
typename Timeline<D, Args...>::timestamp Timeline<D, Args...>::getDate() const{
  return date;
}

template<typename D, typename ...Args>
bool Timeline<D, Args...>::isStable() const{
  return pqueue.empty();
}

/*******************************************************************************
  Timeline::Event
*******************************************************************************/

template<typename D, typename ...Args>
Timeline<D, Args...>::Event::Event
  (Timeline<D, Args...>::timestamp date_occ, const D& data,
   Timeline<D, Args...>::process& on_event):
  date_occ(date_occ), data(data), on_event(&on_event) {}

template<typename D, typename ...Args>
void Timeline<D, Args...>::Event::resolve
  (Args... args, Timeline<D, Args...>& timeline)
{
  on_event(args..., timeline, std::move(data));
}

template<typename D, typename ...Args>
typename Timeline<D, Args...>::timestamp
  Timeline<D, Args...>::Event::getDate() const
{
  return date_occ;
}
