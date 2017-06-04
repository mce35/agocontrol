#!/usr/bin/python

# Filter class, used to store a series of samples and determine if
# a new sample is noise or real sample before adding it to the series

import time
from time import mktime

# 1/31/2016 5:22 AM	3.6
# 1/31/2016 5:16 AM	3.5
# 1/31/2016 5:10 AM	3.6
# 1/31/2016 4:52 AM	3.7
# 1/31/2016 4:45 AM	3.8
# 1/31/2016 4:42 AM	3.9
# 1/31/2016 4:42 AM	22.3
# 1/31/2016 4:40 AM	3.9
# 1/31/2016 4:39 AM	4
# 1/31/2016 4:18 AM	4.1
# 1/31/2016 4:11 AM	4
# 1/31/2016 4:10 AM	22.4
# 1/31/2016 4:08 AM	4
# 1/31/2016 4:04 AM	3.9
# 1/31/2016 4:01 AM	4
# 1/31/2016 3:44 AM	3.9
# 1/31/2016 3:34 AM	3.8
# 1/31/2016 2:53 AM	3.7
# 1/31/2016 2:36 AM	3.6
# 1/31/2016 2:26 AM	3.5
# 1/31/2016 2:19 AM	3.4
# 1/31/2016 2:18 AM	3.3
# 1/31/2016 2:12 AM	3.4
# 1/31/2016 2:09 AM	3.5
# 1/31/2016 1:59 AM	3.6
# 1/31/2016 1:53 AM	3.5
# 1/31/2016 1:32 AM	3.6
# 1/31/2016 1:22 AM	3.5
# 1/31/2016 1:14 AM	3.6
# 1/31/2016 1:08 AM	3.5
# 1/31/2016 1:00 AM	3.4
# 1/31/2016 12:56 AM	3.3



from datetime import datetime


class sampleseries():
    def __init__(self, maxitems, maxage):
        """
        @param maxitems: Maximum number of items in the list
        @param maxage:   Maximum age in list, in seconds
        @return:
        """
        self.maxitems = maxitems
        self.maxage = maxage
        self.samples = []
        self.times = []
        self.samples2 = {}

    def load(self):
        self.samples = [3.3, 3.4, 3.5, 3.6, 3.5, 3.6, 3.5, 3.6, 3.5, 3.4, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8, 3.9, 4.0, 3.9,
                        4.0]
        print self.samples
        print ("len of samples=" + str(self.samples.__len__()))

        self.times = [datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 00:56", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 01:00", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 01:08", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 01:14", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 01:22", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 01:32", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 01:53", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 01:59", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 02:09", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 02:12", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 02:18", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 02:19", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 02:26", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 02:36", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 02:53", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 03:34", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 03:44", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 04:01", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 04:04", "%y %b %d %H:%M"))),
                      datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 04:08", "%y %b %d %H:%M")))]
        self.createT(self.samples, self.times)
        print self.samples2
        print self.samples2.__len__()

        # print self.times
        print ("len of times=" + str(self.times.__len__()))

    def createT(self, samples, times):
        for x in range(0, times.__len__()):
            self.samples2[self.times[x]] = self.samples[x]

    def insert(self, sample, tm):
        self.removeold()

        now = datetime.now()
        self.samples.append(sample)
        self.times.append(tm if tm is not None else now)

        print ("Now=" + str(now) + " sample=" + str(sample))

    def removeold(self):
        print ("len of samples=" + str(self.samples.__len__()))
        if self.samples.__len__() >= self.maxitems:
            print "maxitems reached"
            # find oldest, remove it

    def avg(self):
        sum = 0
        for sample in self.samples:
            sum += sample

        return (sum / len(self.samples))

    def test(self, value, timestamp):
        a = self.avg()
        a1 = 0.9 * a
        a2 = 1.1 * a
        return (False if a1 < value or a2 > value else True)


if __name__ == "__main__":
    a = sampleseries(100, 60 * 60)  # 100 items, max 1 hour
    a.load()
    # 1/31/2016 4:10 AM	22.4
    ret = a.test(22.4, datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 04:10", "%y %b %d %H:%M"))))

    print "ret="
    print ret

    x = a.times[1] - a.times[0]
    print x

    a.insert(1.0, datetime.fromtimestamp(mktime(time.strptime("16 Feb 07 04:10", "%y %b %d %H:%M"))))
    # a.insert(1.1)
    # a.insert(1.2)
    # a.insert(1.1)
    # a.insert(-1.1)

    print "avg=" + str(a.avg())
    # print a.times
    # print ("len of times=" + str(a.times.__len__()))


    # print a
