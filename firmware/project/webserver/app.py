from flask import Flask, request, make_response

import schedule_pb2
import time
import os

# create the Flask app
app = Flask(__name__)

@app.route('/getschedule', methods=["GET"])
def query_example():


    schedule = schedule_pb2.Schedule() 
    schedule.version = 1
    
    
    for eh, em, op in parse_schedule():
        evt = schedule.events.add()
        evt.enabled = True
        evt.start_hou = eh
        evt.start_min = em
        evt.op = op
        evt.wday = schedule_pb2.WeekDay.EVR
    
    res = make_response(schedule.SerializeToString(), 200)
    res.mimetype = "text/plain"

    return res


def parse_schedule():
    
    res = []
    
    fd = open("schedule.csv", "r", encoding="utf-8")
    lines = fd.readlines()
    fd.close()

    for l in lines:
    
        l = l.strip()

        stime, duration = l.split(",")
        
        stime = [int(x) for x in stime.split(".")]
        duration = int(duration)
        
        res.append((*stime, schedule_pb2.Operation.OP_OPEN))

        etime = (
            stime[0] + (stime[1] + duration) // 60, 
            (stime[1] + duration) % 60
        )
        
        res.append((*etime, schedule_pb2.Operation.OP_CLOSE))

    return res


if __name__ == '__main__':
    
    #for e in parse_schedule():
    #    print(e)
    
    app.run(debug=False, host="0.0.0.0", port=25000)

