template <typename T>
LOs offset_scan(Read<T> a);
template <typename TO, typename TI>
Read<TO> scan(Read<TI> a);

/* given an array whose values are
   either non-negative or (-1), and whose
   non-negative values are sorted in
   increasing order, replaces all (-1)
   entries with the nearest non-negative
   value to the left */
void fill_right(Write<LO> a);
