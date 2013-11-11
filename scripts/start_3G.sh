#!/bin/sh

dns1=" "
dns2=" "

start_3G_module() 
{
    cd /etc/ppp/peers
    pppd call gprs&
    echo "pppd ok"
    sleep 12
    echo "sleep ok"
#   cp -rf /etc/ppp/resolv.conf /etc/
    sed -n '1p' /etc/resolv.conf > /etc/ppp/primarydns
    sed -n '2p' /etc/resolv.conf > /etc/ppp/seconddns
    dns1=`cut -f 2 -d ' ' /etc/ppp/primarydns`
    dns2=`cut -f 2 -d ' ' /etc/ppp/seconddns`
    echo $dns1
    echo $dns2
}

check_status()
{
    dns1=`cut -f 2 -d ' ' /etc/ppp/primarydns`
    dns2=`cut -f 2 -d ' ' /etc/ppp/seconddns`
    i="0"
    echo $i
    while [ $i -lt 5 ]
    do
      ping -q -s 1 -c 3 $dns1
      if [ "$?" != "0" ]
        then
        ping -q -s 1 -c 3 $dns2
        if [ "$?" != "0" ]
          then
          sleep 1
        else
          echo "gprs module is online"
          echo "gprs_on_line" > /tmp/gprs_info
          return 0
        fi
      else
        echo "gprs module is online"
        echo "gprs_on_line" > /tmp/gprs_info
        return 0
      fi
      i=$[$i+1]
    done
    echo "gprs module is offline"
    echo "gprs_off_line" > /tmp/gprs_info

    return 1
}


start_3G_module;

while true
do
    check_status
    if [ "$?" != "0" ]
    then
      killall pppd
      start_3G_module
      sleep 5
    else
      echo "Check status, 3g is online"
      sleep 5
    fi
done
