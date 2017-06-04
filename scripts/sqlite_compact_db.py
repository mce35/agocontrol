#!/usr/bin/env python
#
# Datalogger SQLITE db compact tool 
#
# copyright (c) 2016 Christoph Jaeger <jaeger@agocontrol.com>
#
# NOTE - THIS IS ALPHA VERSION 
# we calculate avg value of every hour and update last record with AVG value 
#
# HINTS:
# get all uuid's and there row count:
# select uuid, count(uuid) from data group by uuid ORDER BY count(uuid) DESC; 
#
# Optimize DB:
# sqlite3 datalogger.db ".dump" | sqlite3 optimized_datalogger.db


import sqlite3
import itertools as it
import datetime

# path to your database
sqlite_file = '/var/opt/agocontrol/datalogger.db'
# table name
table = 'data'

# start date
date_start = "2016-08-02"
# end date
date_end = "2016-08-03"
# uuid to optimize
uuid = "7a42d014-9859-45cd-b2f9-5dcc0a86097b"



def sql_avg_values( day, hour_begin, hour_end, uuid ): 
	conn = sqlite3.connect(sqlite_file)
	c = conn.cursor()

	c.execute('SELECT id, uuid, environment, printf("%.2f", level) AS level_avg, strftime("%Y-%m-%d %H:%M:%S", datetime(timestamp, "unixepoch")) AS datum FROM {tn} WHERE datum BETWEEN "{day:%Y-%m-%d} {hb:02d}:00:00" AND "{day:%Y-%m-%d} {he:02d}:00:00" AND uuid="{uuid}"'.\
		format(tn=table, day=day, hb=hour_begin, he=hour_end, uuid=uuid))
	all_rows = c.fetchall()
	print('== RAW DATA - ONE HOUR ==')
	for col in all_rows:
		print('{0} : {1}, {2}, {3}, {4}'.format(col[0], col[1], col[2], col[3], col[4]))


	if len(all_rows)==0:
		print('ERR: No data at this time range')
	else:
		print('== AVERAGE - ONE HOUR ==')
		c.execute('SELECT id, uuid, environment, printf("%.2f", AVG(level)) AS level_avg, strftime("%Y-%m-%d %H:%M:%S", datetime(timestamp, "unixepoch")) AS datum FROM {tn} WHERE datum BETWEEN "{day:%Y-%m-%d} {hb:02d}:00:00" AND "{day:%Y-%m-%d} {he:02d}:00:00" AND uuid="{uuid}" GROUP BY strftime("%H", datum)'.\
			format(tn=table, day=day, hb=hour_begin, he=hour_end, uuid=uuid))
		avg_hour_res = c.fetchall()
		for col in avg_hour_res:
			print('{0} : {1}, {2}, {3}, {4}'.format(col[0], col[1], col[2], col[3], col[4]))
			c.execute("UPDATE data SET level=? WHERE id=?", (col[3], col[0])) 

		print('== IDs to DELETE - ONE HOUR ==')
		for col in all_rows[:-1]:
			#print('{0}'.format(col[0]))
			c.execute('DELETE FROM {tn} WHERE id="{id}"'.\
				format(tn=table, id=col[0]))

	conn.commit()
	conn.close()



# get full range incl. the last item
time_range = lambda start, end: range(start, end+1)

start = datetime.datetime.strptime(date_start, "%Y-%m-%d")
end = datetime.datetime.strptime(date_end, "%Y-%m-%d")
date_generated = [start + datetime.timedelta(days=x) for x in time_range(0, (end-start).days)]

for date in date_generated:
	for hour_begin, hour_end in zip(time_range(00, 23), time_range(01, 24)):
		print "================================= NEW JOB ================================================"
		print "PROCESS day {date:%Y-%m-%d} at HOUR {hour_begin:02d} to {hour_end:02d}".format(date=date, hour_begin=hour_begin, hour_end=hour_end)
		sql_avg_values(date, hour_begin, hour_end, uuid)

