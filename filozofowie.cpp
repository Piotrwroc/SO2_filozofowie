#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <random>
#include <chrono>
#include <string>
#include <condition_variable>
#include <limits>
#include <ncurses.h> 
#include <unistd.h>  

const int NUM_FILOZOFOW = 5;
const int NUM_PALECZEK = NUM_FILOZOFOW;

std::vector<std::mutex> paleczki(NUM_PALECZEK);

std::vector<int> stan_paleczek(NUM_PALECZEK, 0);
std::mutex mutex_stan_paleczek;

std::mutex mutex_ekran;

std::mutex mutex_liczba_jedzacych;
int liczba_jedzacych = 0;
std::condition_variable cv_liczba_jedzacych;

std::vector<int> liczba_posilkow(NUM_FILOZOFOW, 0);
std::mutex mutex_liczba_posilkow;

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> czas_jedzenia(1, 4);
std::uniform_int_distribution<> czas_myslenia(2, 5);

// Funkcja do losowania czasu (w mikrosekundach)
int losuj_czas(std::uniform_int_distribution<>& dyst)
{
    return dyst(gen) * 1000; 
}

void wyswietl_stan(const std::vector<std::string>& stan_filozofow, int strategia)
{
    std::lock_guard<std::mutex> lock(mutex_ekran);
    clear(); 
    mvprintw(0, 0, "Symulacja Problemu Pieciu Filozofow wersja %d", strategia);
    for (int i = 0; i < NUM_FILOZOFOW; ++i)
    {
        mvprintw(i + 1, 0, "Filozof %d: %s (Zjadl: %d)", i + 1, stan_filozofow[i].c_str(), liczba_posilkow[i]);
    }
    mvprintw(NUM_FILOZOFOW + 2, 0, "Stan paleczek:");
    for (int i = 0; i < NUM_PALECZEK; ++i)
    {
        mvprintw(NUM_FILOZOFOW + 3 + i, 0, "Paleczka %d: ", i + 1);
        if (stan_paleczek[i] == 0)
        {
            printw("Wolna");
        }
        else
        {
            printw("Uzywana przez Filozofa %d", stan_paleczek[i]);
        }
    }
    refresh(); 
}

void filozof(int id, int strategia, std::vector<std::string>& stan_filozofow)
{
    while (true)
    {
        int lewa_paleczka = id;
        int prawa_paleczka = (id + 1) % NUM_PALECZEK;

        stan_filozofow[id] = "Glodny";
        wyswietl_stan(stan_filozofow, strategia);

        if (strategia == 1) // Wersja z zakleszczeniem 
        {
            paleczki[lewa_paleczka].lock();
            {
                std::lock_guard<std::mutex> lock_stan(mutex_stan_paleczek);
                stan_paleczek[lewa_paleczka] = id + 1;
            }
            wyswietl_stan(stan_filozofow, strategia);
            usleep(300 * 1000); 

            paleczki[prawa_paleczka].lock();
            {
                std::lock_guard<std::mutex> lock_stan(mutex_stan_paleczek);
                stan_paleczek[prawa_paleczka] = id + 1;
            }

            stan_filozofow[id] = "Je";
            {
                std::lock_guard<std::mutex> lock(mutex_liczba_posilkow);
                liczba_posilkow[id]++;
            }
            wyswietl_stan(stan_filozofow, strategia);
            usleep(losuj_czas(czas_jedzenia) * 1000);


            {
                std::lock_guard<std::mutex> lock_stan(mutex_stan_paleczek);
                stan_paleczek[lewa_paleczka] = 0;
                stan_paleczek[prawa_paleczka] = 0;
            }
            paleczki[lewa_paleczka].unlock();
            paleczki[prawa_paleczka].unlock();
        }

        else if (strategia == 2)  // Wersja z zaglodzeniem 
        {
            if (id == NUM_FILOZOFOW - 1) 
            {
                paleczki[prawa_paleczka].lock();
                {
                    std::lock_guard<std::mutex> lock_stan(mutex_stan_paleczek);
                    stan_paleczek[prawa_paleczka] = id + 1;
                }
                usleep(50* 100 * 1000);  

                if (!paleczki[lewa_paleczka].try_lock())
                {
                    paleczki[prawa_paleczka].unlock();
                    usleep(losuj_czas(czas_myslenia) * 1000); 
                    continue;
                }
                {
                    std::lock_guard<std::mutex> lock_stan(mutex_stan_paleczek);
                    stan_paleczek[lewa_paleczka] = id + 1;
                }
            }
            else 
            {
                paleczki[lewa_paleczka].lock();
                {
                    std::lock_guard<std::mutex> lock_stan(mutex_stan_paleczek);
                    stan_paleczek[lewa_paleczka] = id + 1;
                }
                usleep(50 * 1000);
                paleczki[prawa_paleczka].lock();
                {
                    std::lock_guard<std::mutex> lock_stan(mutex_stan_paleczek);
                    stan_paleczek[prawa_paleczka] = id + 1;
                }
            }

            stan_filozofow[id] = "Je";
            {
                std::lock_guard<std::mutex> lock(mutex_liczba_posilkow);
                liczba_posilkow[id]++;
            }
            wyswietl_stan(stan_filozofow, strategia);
            usleep(losuj_czas(czas_jedzenia) * 1000);

            {
                std::lock_guard<std::mutex> lock_stan(mutex_stan_paleczek);
                stan_paleczek[lewa_paleczka] = 0;
                stan_paleczek[prawa_paleczka] = 0;
            }
            paleczki[lewa_paleczka].unlock();
            paleczki[prawa_paleczka].unlock();
        }
        
        else if (strategia == 3)  // Wersja z zabezpieczeniami 
        {
            std::unique_lock<std::mutex> lock_liczba(mutex_liczba_jedzacych);
            cv_liczba_jedzacych.wait(lock_liczba, [&] { return liczba_jedzacych < NUM_FILOZOFOW - 1; });
            liczba_jedzacych++;
            lock_liczba.unlock();

            int pierwsza_paleczka = std::min(lewa_paleczka, prawa_paleczka);
            int druga_paleczka = std::max(lewa_paleczka, prawa_paleczka);

            paleczki[pierwsza_paleczka].lock();
            {
                std::lock_guard<std::mutex> lock_stan(mutex_stan_paleczek);
                stan_paleczek[pierwsza_paleczka] = id + 1;
            }
            paleczki[druga_paleczka].lock();
            {
                std::lock_guard<std::mutex> lock_stan(mutex_stan_paleczek);
                stan_paleczek[druga_paleczka] = id + 1;
            }

            stan_filozofow[id] = "Je";
            {
                std::lock_guard<std::mutex> lock(mutex_liczba_posilkow);
                liczba_posilkow[id]++;
            }
            wyswietl_stan(stan_filozofow, strategia);
            usleep(losuj_czas(czas_jedzenia) * 1000);

            {
                std::lock_guard<std::mutex> lock_stan(mutex_stan_paleczek);
                stan_paleczek[pierwsza_paleczka] = 0;
                stan_paleczek[druga_paleczka] = 0;
            }
            paleczki[pierwsza_paleczka].unlock();
            paleczki[druga_paleczka].unlock();

            std::lock_guard<std::mutex> lock_liczba_zwolnienie(mutex_liczba_jedzacych);
            liczba_jedzacych--;
            cv_liczba_jedzacych.notify_one();
        }

        stan_filozofow[id] = "Mysli";
        wyswietl_stan(stan_filozofow, strategia);
        usleep(losuj_czas(czas_myslenia) * 1000);
    }
}

int main()
{
    int strategia;
    std::vector<std::string> stan_filozofow(NUM_FILOZOFOW);

    initscr();
    cbreak();             
    noecho();             
    keypad(stdscr, TRUE); 
    nodelay(stdscr, TRUE); 

    while (true)
    {
        clear();
        mvprintw(0, 0, "Wybierz strategie:");
        mvprintw(1, 0, "1. Wersja z zakleszczeniem");
        mvprintw(2, 0, "2. Wersja z zaglodzeniem");
        mvprintw(3, 0, "3. Wersja z zapobieganiem");
        refresh();

        int ch = getch();
        if (ch == '1')
        {
            strategia = 1;
            break;
        }
        else if (ch == '2')
        {
            strategia = 2;
            break;
        }
        else if (ch == '3')
        {
            strategia = 3;
            break;
        }
        usleep(100 * 1000); 
    }

    std::vector<std::thread> filozofowie(NUM_FILOZOFOW);
    for (int i = 0; i < NUM_FILOZOFOW; ++i)
    {
        filozofowie[i] = std::thread(filozof, i, strategia, std::ref(stan_filozofow));
    }

    while (true)
    {
        wyswietl_stan(stan_filozofow, strategia);
        usleep(500 * 1000); 
        if (getch() ==  27) 
        {
            break;
        }
    }

    endwin();

    for (auto& f : filozofowie)
    {
        if (f.joinable())
        {
            f.detach();
        }
    }
    return 0;
}