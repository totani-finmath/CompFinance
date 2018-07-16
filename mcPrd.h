
/*
Written by Antoine Savine in 2018

This code is the strict IP of Antoine Savine

License to use and alter this code for personal and commercial applications
is freely granted to any person or company who purchased a copy of the book

Modern Computational Finance: AAD and Parallel Simulations
Antoine Savine
Wiley, 2018

As long as this comment is preserved at the top of the file
*/

#pragma once

#include <map>

#include "mcBase.h"

#define ONE_HOUR 0.000114469
#define ONE_DAY 0.003773585

template <class T>
class European : public Product<T>
{
    double              myStrike;
    Time                myExerciseDate;
    Time                mySettlementDate;

    vector<Time>        myTimeline;
    vector<SimulDef>   myDataline;

    vector<string>      myLabels;

public:

    //  Constructor: store data and build timeline
    European(const double strike, 
        const Time exerciseDate,
        const Time settlementDate) :
        myStrike(strike),
        myExerciseDate(exerciseDate),
        mySettlementDate(settlementDate),
        myLabels(1)
    {
        //  Timeline = { exercise date }
        myTimeline.push_back(exerciseDate);

        //  Dataline
        myDataline.resize(1);   //  only exercise date
        //  Numeraire needed
        myDataline[0].numeraire = true;
        //  Forward to settlement needed at exercise
        myDataline[0].forwardMats.push_back(settlementDate);
        //  Discount to settlement needed at exercise
        myDataline[0].discountMats.push_back(settlementDate);

        //  Identify the product
        ostringstream ost;
        ost.precision(2);
        ost << fixed;
        if (settlementDate == exerciseDate)
        {
            ost << "call " << myStrike << " " << exerciseDate;
        }
        else
        {
            ost << "call " << myStrike << " " << exerciseDate << " " << settlementDate;
        }
        myLabels[0] = ost.str();
    }

    European(const double strike,
        const Time exerciseDate) : European(strike, exerciseDate, exerciseDate)
    {}

    //  Virtual copy constructor
    unique_ptr<Product<T>> clone() const override
    {
        return unique_ptr<Product<T>>(new European<T>(*this));
    }

    //  Timeline
    const vector<Time>& timeline() const override
    {
        return myTimeline;
    }

    //  Dataline
    const vector<SimulDef>& dataline() const override
    {
        return myDataline;
    }

    //  Labels
    const vector<string>& payoffLabels() const override
    {
        return myLabels;
    }

    //  Payoffs, maturity major
    void payoffs(
        //  path, one entry per time step (on the product timeline)
        const Scenario<T>&          path,
        //  pre-allocated space for resulting payoffs
        vector<T>&                  payoffs)
            const override
    {
        payoffs[0] = max(path[0].forwards[0] - myStrike, 0.0)
            * path[0].discounts[0]
            / path[0].numeraire; 
    }
};

template <class T>
class UOC : public Product<T>
{
    double              myStrike;
    double              myBarrier;
    Time                myMaturity;
    
    double              mySmooth;
    
    vector<Time>        myTimeline;
    vector<SimulDef>   myDataline;

    vector<string>      myLabels;

public:

    //  Constructor: store data and build timeline
    //  Timeline = system date to maturity, 
    //  with steps every monitoring frequency
    UOC(const double    strike, 
        const double    barrier, 
        const Time      maturity, 
        const Time      monitorFreq,
        const double    smooth)
        : myStrike(strike), 
        myBarrier(barrier), 
        myMaturity(maturity),
        mySmooth(smooth),
        myLabels(2)
    {
        //  Produce timeline

        //  Today
        myTimeline.push_back(systemTime);
        Time t = systemTime + monitorFreq;
            
        //  Barrier monitoring
        while (myMaturity - t > ONE_HOUR)
        {
            myTimeline.push_back(t);
            t += monitorFreq;
        }

        //  Maturity
        myTimeline.push_back(myMaturity);

        //

        //  Dataline

        const size_t n = myTimeline.size();
        myDataline.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            //  Numeraire needed only on last step
            myDataline[i].numeraire = false;

            //  spot(t) = forward (t, t) needed on every step
            myDataline[i].forwardMats.push_back(myTimeline[i]);
        }
        //  Numeraire needed only on last step
        myDataline.back().numeraire = true;

        //

        //  Identify the product
        ostringstream ost;
        ost.precision(2);
        ost << fixed;
        ost << "call " << myMaturity << " " << myStrike ;
        myLabels[1] = ost.str();

        ost << " up and out "
            << myBarrier << " monitoring freq " << monitorFreq
            << " smooth " << mySmooth;
        myLabels[0] = ost.str();
    }

    //  Virtual copy constructor
    unique_ptr<Product<T>> clone() const override
    {
        return unique_ptr<Product<T>>(new UOC<T>(*this));
    }

    //  Timeline
    const vector<Time>& timeline() const override
    {
        return myTimeline;
    }

    //  Dataline
    const vector<SimulDef>& dataline() const override
    {
        return myDataline;
    }

    //  Labels
    const vector<string>& payoffLabels() const override
    {
        return myLabels;
    }

    //  Payoff
    void payoffs(
        //  path, one entry per time step (on the product timeline)
        const Scenario<T>&          path,
        //  pre-allocated space for resulting payoffs
        vector<T>&                  payoffs)
            const override
    {
        //  We apply the smooth barrier technique to stabilize risks
        //  See Savine's presentation on Fuzzy Logic, Global Derivatives 2016
        //  Or Andreasen and Savine's publication on scripting

        //  We apply a smoothing factor of x% of the spot both ways, untemplated
        const double smooth = convert<double>(path[0].forwards[0] * mySmooth),
            twoSmooth = 2 * smooth,
            barSmooth = myBarrier + smooth;

        //  We start alive
        T alive(1.0);

        //  Go through path, update alive status
        for (const auto& scen: path)
        {
            //  Breached
            if (scen.forwards[0] > barSmooth)
            {
                alive = T(0.0);
                break;
            }

            //  Semi-breached: apply smoothing
            if (scen.forwards[0] > myBarrier - smooth)
            {
                alive *= (barSmooth - scen.forwards[0]) / twoSmooth;
            }
        }

        //  Payoff
        payoffs[1] = max(path.back().forwards[0] - myStrike, 0.0) / path.back().numeraire;
        payoffs[0] = alive * payoffs[1];
    }
};

template <class T>
class Europeans : public Product<T>
{
    vector<Time>            myMaturities;   //  = timeline
    vector<vector<double>>  myStrikes;      //  a vector of strikes per maturity
    vector<SimulDef>       myDataline;

    vector<string>          myLabels;

public:

    //  Constructor: store data and build timeline
    Europeans(const map<Time, vector<double>>& options) 
    {
        const size_t n = options.size();

        //  Timeline = one step per maturity
        for (const pair<Time, vector<double>>& p : options)
        {
            myMaturities.push_back(p.first);
            myStrikes.push_back(p.second);
        }

        //  Dataline = num and spot(t) = forward(t,t) on every step
        myDataline.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            myDataline[i].numeraire = true;
            myDataline[i].forwardMats.push_back(myMaturities[i]);
        }

        //  Identify the payoffs
        for (const auto& option : options)
        {
            for (const auto& strike : option.second)
            {
                ostringstream ost;
                ost.precision(2);
                ost << fixed;
                ost << "call " << option.first << " " << strike;
                myLabels.push_back(ost.str());
            }
        }
    }

    //  access to maturities and strikes
    const vector<Time>& maturities() const
    {
        return myMaturities;
    }

    const vector<vector<double>>& strikes() const
    {
        return myStrikes;
    }

    //  Virtual copy constructor
    unique_ptr<Product<T>> clone() const override
    {
        return unique_ptr<Product<T>>(new Europeans<T>(*this));
    }

    //  Timeline
    const vector<Time>& timeline() const override
    {
        return myMaturities;
    }

    //  Dataline
    const vector<SimulDef>& dataline() const override
    {
        return myDataline;
    }

    //  Labels
    const vector<string>& payoffLabels() const override
    {
        return myLabels;
    }

    //  Payoffs, maturity major
    void payoffs(
        //  path, one entry per time step (on the product timeline)
        const Scenario<T>&          path,
        //  pre-allocated space for resulting payoffs
        vector<T>&                  payoffs)
        const override
    {
        const size_t numT = myMaturities.size(), numK = myStrikes.size();

        auto payoffIt = payoffs.begin();
        for (size_t i = 0; i < numT; ++i)
        {
            transform(
                    myStrikes[i].begin(),
                    myStrikes[i].end(),
                    payoffIt,
                    [spot = path[i].forwards[0], num = path[i].numeraire] (const double& k) 
                    {
                        return max(spot - k, 0.0) / num; 
                    }
            );

            payoffIt += myStrikes[i].size();
        }
    }
};

//  Payoff = sum { (libor(Ti, Ti+1) + cpn) * coverage(Ti, Ti+1) only if Si+1 >= Si }
template <class T>
class ContingentBond : public Product<T>
{
    Time                myMaturity;
    double              myCpn;
    double              mySmooth;

    vector<Time>        myTimeline;
    vector<SimulDef>   myDataline;

    vector<string>      myLabels;

    //  Pre-computed coverages
    vector<double>      myDt;

public:

    //  Constructor: store data and build timeline
    //  Timeline = system date to maturity, 
    //  with steps every payment frequency
    ContingentBond(
        const Time      maturity,
        const double    cpn,
        const Time      payFreq,
        const double    smooth)
        : 
        myMaturity(maturity),
        myCpn(cpn),
        mySmooth(smooth),
        myLabels(1)
    {
        //  Produce timeline

        //  Today
        myTimeline.push_back(systemTime);
        Time t = systemTime + payFreq;

        //  Payment schedule
        while (myMaturity - t > ONE_DAY)
        {
            myDt.push_back(t - myTimeline.back());
            myTimeline.push_back(t);
            t += payFreq;
        }

        //  Maturity
        myDt.push_back(myMaturity - myTimeline.back());
        myTimeline.push_back(myMaturity);

        //

        //  Dataline

        //  Payoff = sum { (libor(Ti, Ti+1) + cpn) * coverage(Ti, Ti+1) only if Si+1 >= Si }
        //  We need spot ( = forward (Ti, Ti) ) on every step,
        //  and libor (Ti, Ti+1) on on every step but the last
        //  (coverage is assumed act/365)

        const size_t n = myTimeline.size();
        myDataline.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            //  spot(Ti) = forward (Ti, Ti) needed on every step
            myDataline[i].forwardMats.push_back(myTimeline[i]);

            //  libor(Ti, Ti+1) and discount (Ti, Ti+1) needed on every step but last
            if (i < n - 1)
            {
                myDataline[i].liborDefs.push_back(
                    SimulDef::RateDef(myTimeline[i], myTimeline[i + 1], "libor"));
            }

            //  Numeraire needed only on every step but first
            myDataline[i].numeraire = i > 0;
        }

        //  Identify the product
        ostringstream ost;
        ost.precision(2);
        ost << fixed;
        ost << "contingent bond " << myMaturity << " " << myCpn;
        myLabels[0] = ost.str();
    }

    //  Virtual copy constructor
    unique_ptr<Product<T>> clone() const override
    {
        return unique_ptr<Product<T>>(new ContingentBond<T>(*this));
    }

    //  Timeline
    const vector<Time>& timeline() const override
    {
        return myTimeline;
    }

    //  Dataline
    const vector<SimulDef>& dataline() const override
    {
        return myDataline;
    }

    //  Labels
    const vector<string>& payoffLabels() const override
    {
        return myLabels;
    }

    //  Payoff
    void payoffs(
        //  path, one entry per time step (on the product timeline)
        const Scenario<T>&          path,
        //  pre-allocated space for resulting payoffs
        vector<T>&                  payoffs)
        const override
    {
        //  We apply the smooth digital technique to stabilize risks
        //  See Savine's presentation on Fuzzy Logic, Global Derivatives 2016
        //  Or Andreasen and Savine's publication on scripting

        //  We apply a smoothing factor of x% of the spot both ways, untemplated
        const double smooth = convert<double>(path[0].forwards[0] * mySmooth),
            twoSmooth = 2 * smooth;

        //  We start alive
        T alive(1.0);

        //  Go through path, update alive status
        //  Period by period
        const size_t n = path.size() - 1;
        payoffs[0] = 0;
        for (size_t i = 0; i < n; ++i)
        {
            const auto& start = path[i];
            const auto& end = path[i + 1];

            const T s0 = start.forwards[0], s1 = end.forwards[0];

            //  Is asset performance positive?

            /*
                We apply smoothing here otherwise risks are unstable with bumps
                    and wrong with AAD 

                bool digital = end.forwards[0] >= start.forwards[0];
            */

            T digital;
            if (s1 - s0 > smooth)
            {
                digital = T(1.0);
            }
            else if (s1 - s0 < - smooth)
            {
                digital = T(0.0);
            }
            else // "fuzzy" barrier = interpolate
            {
                digital = (s1 - s0 + smooth) / twoSmooth;
            }

            //  ~smoothing

            payoffs[0] += 
                digital             //  contingency
                * ( start.libors[0] //  libor(Ti, Ti+1)
                + myCpn)            //  + coupon
                * myDt[i]           //  day count / 365
                / end.numeraire;    //  paid at Ti+1
        }
        payoffs[0] += 1.0 / path.back().numeraire;  //  redemption at maturity
    }
};
