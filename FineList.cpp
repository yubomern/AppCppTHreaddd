#include "FineList.hpp"


int main()
{
    FineList<int>* list = new FineList<int>;
    list->add(1);
    bool a = list->contains(1);
    cout << a << "\n";
    bool remove = list->remove(1);
    cout << remove << "\n";
    a = list->contains(1);
    cout << a << "\n";

    delete list;

    return 0;
}