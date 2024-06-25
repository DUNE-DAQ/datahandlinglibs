// Declarations for BinarySearchQueueModel

namespace dunedaq {
namespace datahandlinglibs {

template<typename T>
typename IterableQueueModel<T>::Iterator 
BinarySearchQueueModel<T>::lower_bound(T& element, bool /*with_errors=false*/)
{
  unsigned int start_index =
    IterableQueueModel<T>::readIndex_.load(std::memory_order_relaxed); // NOLINT(build/unsigned)
  unsigned int end_index =
    IterableQueueModel<T>::writeIndex_.load(std::memory_order_acquire); // NOLINT(build/unsigned)

  if (start_index == end_index) {
    TLOG() << "Queue is empty" << std::endl;
    return IterableQueueModel<T>::end();
  }
  end_index = end_index == 0 ? IterableQueueModel<T>::size_ - 1 : end_index - 1;

  T& left_element = IterableQueueModel<T>::records_[start_index];

  if (element < left_element) {
    TLOG() << "Could not find element" << std::endl;
    return IterableQueueModel<T>::end();
  }

  while (true) {
    unsigned int diff =
      start_index <= end_index ? end_index - start_index : IterableQueueModel<T>::size_ + end_index - start_index;
    unsigned int middle_index = start_index + ((diff + 1) / 2);
    if (middle_index >= IterableQueueModel<T>::size_)
      middle_index -= IterableQueueModel<T>::size_;
    T& element_between = IterableQueueModel<T>::records_[middle_index];

    //if we landed on our element, let's get out of here.
    if (element.get_first_timestamp()==element_between.get_first_timestamp())
      return typename IterableQueueModel<T>::Iterator(*this, middle_index);

    if ( diff == 0 ) {

      //if we satisfy the lower_bound condition, we have the right index
      if(element < element_between)
	return typename IterableQueueModel<T>::Iterator(*this, middle_index);

      //if we don't, we need to increment one up. for safety check size too
      if(++middle_index >= IterableQueueModel<T>::size_)
	middle_index -= IterableQueueModel<T>::size_;
      
      return typename IterableQueueModel<T>::Iterator(*this, middle_index);
    }
    
    if (element < element_between) {
      end_index = middle_index != 0 ? middle_index - 1 : IterableQueueModel<T>::size_ - 1;
    } else {
      start_index = middle_index;
    }
  }
}

} // namespace datahandlinglibs
} // namespace dunedaq
